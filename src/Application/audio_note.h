/* Simple tone playback helper for MT6261 using existing AFE Beep generator */
#ifndef AUDIO_NOTE_H
#define AUDIO_NOTE_H

#include "systemconfig.h"

/* Initialize audio front-end once. Safe to call multiple times. */
void AudioNote_Initialize(void);

/* Play a short fixed beep (non-blocking trigger). */
void AudioNote_PlayBeep(void);

/* Play a blocking note with approximate frequency (Hz) and duration (ms).
 * Uses DAC test sinus generator divisors; frequency mapping is approximate.
 */
void AudioNote_PlayTone(uint32_t freq_hz, uint32_t duration_ms);

#endif /* AUDIO_NOTE_H */
