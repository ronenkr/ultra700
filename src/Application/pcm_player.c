#include <stdint.h>
#include <stddef.h>
#include "systemconfig.h"
#include "afe.h"
#include "pcm_player.h"
#include "pmu.h"          // if you have backlight / audio LDO control here
#include "usb_print.h"

// ---- Minimal AFE register access (adjust if your headers already define) ----
#ifndef AFE_BASE
#define AFE_BASE        0xA07C0000u
#endif
#define AFE_DAC_CON0    (*(volatile uint32_t*)(AFE_BASE + 0x0000))
#define AFE_DAC_CON1    (*(volatile uint32_t*)(AFE_BASE + 0x0004))
#define AFE_DL_FIFO     (*(volatile uint32_t*)(AFE_BASE + 0x0070))
#define AFE_DL_STATUS   (*(volatile uint32_t*)(AFE_BASE + 0x0074))

// Bit masks (verify against mt6261 docs)
#ifndef AFE_DAC_CON0_DL_EN
#define AFE_DAC_CON0_DL_EN    (1u<<0)
#define AFE_DAC_CON0_DAC_EN   (1u<<1)
#endif
#ifndef AFE_STATUS_DL_FIFO_FULL
#define AFE_STATUS_DL_FIFO_FULL (1u<<1)
#endif

void AFE_TurnOn8K(int path)
{
    (void)path;
    // Enable voice IRQ + DL path if not already
    AFE_VMCU_CON0 |= VIRQON;              // voice interrupt (optional)
    AFE_MCU_CON1 = (AFE_MCU_CON1 & ~AFE_PATH_MASK) | UDSP_DL_ON | A_IF_DL_ON;
    AFE_MCU_CON0 |= AFE_ON;
    // Unmute path if mute bits engaged (check MUTE registers)
    // Example clearing mute (depends on your chip):
    // ABBA_AUDIODL_CON1 |= 0x6000;  // ensure ZCD enable
}

void AFE_TurnOff8K(int path)
{
    (void)path;
    AFE_MCU_CON1 &= ~AFE_PATH_MASK;
    AFE_MCU_CON0 &= ~AFE_ON;
}


void AFE_SetOutputVolume(uint8_t left, uint8_t right) {
    (void)right;
    AFE_SetSpeakerVolume(left);
}


// Fallback defines if not present:
#ifndef DL_PATH
#define DL_PATH 0
#endif

// ----- Add a simple generated tone buffer to prove path first -----
static void gen_sine16(int16_t *buf, uint32_t samples, uint32_t fs, uint32_t f)
{
    uint32_t phase = 0;
    uint32_t inc = (uint64_t)f * 65536ull / fs;
    for (uint32_t i=0;i<samples;i++) {
        // 15-bit amplitude
        int16_t s = (int16_t)(( (int32_t) ( (phase >> 1) & 0x7FFF) - 0x3FFF ));
        buf[i] = s;
        phase += inc;
    }
}

static int g_inited = 0;
static uint32_t g_last_rate = 0;

// Re-check correct status bit: assume bit1 FULL; if not, we accept any small wait loop instead.
static inline void fifo_wait_space(void)
{
    // If wrong bit, this loop will spin too long. Add timeout to detect.
    uint32_t guard = 100000;
    while ((AFE_DL_STATUS & AFE_STATUS_DL_FIFO_FULL) && --guard);
}

// Stronger init: ensure analog + path
static void pcm_hw_enable(uint32_t sample_rate)
{
    if (!g_inited) {
        AFE_initialize();          // your existing core init
        g_inited = 1;
    }

    // Force sample rate programming if afe.c exposes API (else ignore)
    if (sample_rate != g_last_rate) {
        // (If you have AFE_SetSampleRate use it; placeholder here)
        g_last_rate = sample_rate;
    }

    // Turn on 8K path (even if using 16K test, just to open routing)
    AFE_TurnOn8K(DL_PATH);

    // Unmute + set moderate volume (if APIs exist)
    AFE_Unmute();

    AFE_EnsureOutputPath();
#ifdef AFE_SetOutputVolume
    AFE_SetOutputVolume(200,200);
#endif

    // Enable DL + DAC bits
    AFE_DAC_CON0 |= (AFE_DAC_CON0_DL_EN | AFE_DAC_CON0_DAC_EN);
}

static void pcm_hw_disable(void)
{
    AFE_DAC_CON0 &= ~(AFE_DAC_CON0_DL_EN | AFE_DAC_CON0_DAC_EN);
    AFE_TurnOff8K(DL_PATH);
}

// Play raw PCM (16-bit mono)
void PCM_Player_PlayBuffer(const int16_t *buf, uint32_t samples, uint32_t sample_rate)
{
    if (!buf || samples == 0) return;
    pcm_hw_enable(sample_rate);

    for (uint32_t i=0; i<samples; i++) {
        fifo_wait_space();
        AFE_DL_FIFO = (uint16_t)buf[i];
    }

    // Drain approximate time
    uint32_t us = (uint64_t)samples * 1000000ull / sample_rate;
    USC_Pause_us(us + 3000);

    pcm_hw_disable();
}

// Self test tone (prior to using demo_pcm)
void PCM_Player_PlayTestTone(uint32_t freq_hz, uint32_t ms)
{
    const uint32_t fs = 8000;
    static int16_t tmp[1024];
    uint32_t total = (uint64_t)ms * fs / 1000;
    pcm_hw_enable(fs);
    while (total) {
        uint32_t chunk = (total > 1024) ? 1024 : total;
        gen_sine16(tmp, chunk, fs, freq_hz);
        for (uint32_t i=0; i<chunk; i++) {
            fifo_wait_space();
            AFE_DL_FIFO = (uint16_t)tmp[i];
        }
        total -= chunk;
    }
    USC_Pause_us(2000);
    pcm_hw_disable();
    USB_Print("PCM: test tone done\n");
}

// Original simple sample fallback
static const int16_t demo_pcm[] = {
     0,  4096,  8192, 12288, 16384, 12288,  8192,  4096,    0,-4096,-8192,-12288,-16384,-12288,-8192,-4096,
     0,  4096,  8192, 12288, 16384, 12288,  8192,  4096,    0,-4096,-8192,-12288,-16384,-12288,-8192,-4096,
     0,  4096,  8192, 12288, 16384, 12288,  8192,  4096,    0,-4096,-8192,-12288,-16384,-12288,-8192,-4096
};

void PCM_Player_PlaySample(void)
{
    // First try a test tone to verify path; comment out once working
    PCM_Player_PlayTestTone(1000, 300);
    PCM_Player_PlayBuffer(demo_pcm, (uint32_t)(sizeof(demo_pcm)/sizeof(demo_pcm[0])), 8000);
}

void PCM_Player_Init(void)
{
    if (!g_inited) {
        AFE_initialize();
        g_inited = 1;
        USB_Print("PCM: init\n");
    }
}
