// Simple tone playback helper leveraging existing AFE initialization and Beep() function.
// For now we reuse AFE_initialize() and AFE_DAC_TEST sine generator; extend later for PCM.

#include "systemconfig.h"
#include "afe.h"
#include "audio_note.h"
#include <math.h>   // if not allowed, we use integer table (provided below)

// ===== Explanation =====
// Your current method uses AFE_DAC_TEST with VDAC_SINUS / ADAC_SINUS.
// That internal test tone block usually produces *one fixed reference frequency* (or only a coarse set),
// so changing the "div" bits you wrote (best_div) has no audible effect (wrong field or block ignores it).
// Result: every note sounds identical.
//
// Solution here: generate a software PCM sine at the desired frequency and push it
// to the downlink FIFO (blocking). This guarantees pitch changes without needing
// undocumented test bits.
//
// If you later add DMA, just replace the inner FIFO write loop with a DMA fill.

// ---- Minimal FIFO symbols (adjust if different in afe.h) ----
#ifndef AFE_DL_FIFO
#define AFE_BASE        0xA07C0000
#define AFE_DAC_CON0    (*(volatile uint32_t*)(AFE_BASE + 0x0000))
#define AFE_DAC_CON1    (*(volatile uint32_t*)(AFE_BASE + 0x0004))
#define AFE_DL_FIFO     (*(volatile uint32_t*)(AFE_BASE + 0x0070))
#define AFE_DL_STATUS   (*(volatile uint32_t*)(AFE_BASE + 0x0074))
#endif

#ifndef AFE_DAC_CON0_DL_EN
#define AFE_DAC_CON0_DL_EN   (1u<<0)
#define AFE_DAC_CON0_DAC_EN  (1u<<1)
#define AFE_STATUS_DL_FIFO_FULL (1u<<1)
#endif

// ===== Sine table (one cycle, 256 samples, amplitude 14-bit) =====
static const int16_t sine256[283] = {
    0,804,1607,2407,3203,3993,4776,5550,6314,7066,7805,8529,9237,9928,10600,11252,
    11882,12490,13073,13631,14162,14665,15138,15582,15994,16373,16719,17031,17307,17547,17751,17917,
    18045,18135,18186,18198,18171,18105,18000,17857,17675,17455,17198,16905,16576,16213,15816,15386,
    14926,14435,13916,13369,12796,12199,11578,10936,10274, 9593, 8895, 8182, 7455, 6716, 5966, 5208,
     4442, 3671, 2897, 2120, 1343,  567, -207, -978,-1745,-2507,-3261,-4008,-4746,-5473,-6189,-6893,
    -7583,-8258,-8917,-9559,-10183,-10788,-11373,-11937,-12479,-12998,-13494,-13965,-14411,-14831,-15224,-15589,
    -15926,-16234,-16512,-16761,-16978,-17164,-17318,-17440,-17530,-17587,-17612,-17604,-17563,-17489,-17382,-17242,
    -17069,-16863,-16625,-16355,-16053,-15720,-15355,-14961,-14536,-14083,-13601,-13092,-12556,-11995,-11409,-10800,
    -10168, -9515, -8832, -8129, -7409, -6672, -5920, -5154, -4376, -3588, -2791, -1987, -1177,  -365,   447,  1258,
      2066,  2870,  3667,  4457,  5238,  6008,  6766,  7511,  8241,  8954,  9650, 10326, 10981, 11614,
     12224, 12808, 13366, 13897, 14399, 14871, 15312, 15721, 16096, 16438, 16744, 17014, 17248, 17444, 17603, 17723,
     17805, 17847, 17849, 17812, 17735, 17618, 17462, 17266, 17031, 16757, 16446, 16098, 15714, 15295, 14843, 14359,
     13843, 13299, 12727, 12130, 11509, 10866, 10202,  9519,  8818,  8099,  7366,  6620,  5862,  5094,  4318,  3537,
      2751,  1964,  1176,   389,  -395, -1177, -1954, -2726, -3492, -4249, -4997, -5734, -6458, -7168, -7863, -8542,
     -9203, -9846,-10469,-11071,-11651,-12208,-12742,-13250,-13733,-14189,-14617,-15017,-15388,-15729,-16039,-16318,
    -16565,-16780,-16962,-17111,-17226,-17307,-17354,-17367,-17345,-17289,-17199,-17075,-16918,-16726,-16502,-16245,
    -15956,-15636,-15285,-14904,-14494,-14056,-13591,-13099,-12583,-12043,-11480,-10894,-10289, -9664, -9022, -8364,
     -7692, -7006, -6308, -5599, -4882, -4156, -3424, -2687, -1947, -1204,  -460
};

// Sample rate for tone generation:
#define TONE_SAMPLE_RATE 8000u

// Generate & play a tone (blocking) into FIFO (mono 16-bit)
static void play_tone_pcm(uint32_t freq_hz, uint32_t duration_ms)
{
    if (freq_hz == 0 || duration_ms == 0) return;
    // Ensure AFE path enabled
    AFE_DAC_CON0 |= (AFE_DAC_CON0_DL_EN | AFE_DAC_CON0_DAC_EN);

    // Phase increment (fixed point: 8 fractional bits since table size 256)
    // phase_index = (phase_accum >> 8) & 0xFF
    uint32_t phase_inc = ( (uint32_t)freq_hz * 256u + (TONE_SAMPLE_RATE/2) ) / TONE_SAMPLE_RATE;
    if (phase_inc == 0) phase_inc = 1;

    uint32_t total_samples = (uint64_t)duration_ms * TONE_SAMPLE_RATE / 1000u;
    uint32_t phase = 0;

    for (uint32_t i=0; i<total_samples; i++) {
        // Wait while FIFO full
        while (AFE_DL_STATUS & AFE_STATUS_DL_FIFO_FULL)
            ;
        int16_t s = sine256[(phase >> 0) & 0xFF]; // no extra shift, table index in low 8 bits
        phase += phase_inc;
        // Write sample (sign extend into 32-bit word write)
        AFE_DL_FIFO = (uint16_t)s;
    }
}

// Replace old (test generator) implementation
void AudioNote_PlayTone(uint32_t freq_hz, uint32_t duration_ms)
{
    AudioNote_Initialize();
    play_tone_pcm(freq_hz, duration_ms);
}

static boolean g_audio_initialized = false;

void AudioNote_Initialize(void)
{
    if (g_audio_initialized) return;
    AFE_initialize();
    g_audio_initialized = true;
}

// (Optional) keep Beep() for legacy single fixed beep
// void AudioNote_PlayBeep(void) {
