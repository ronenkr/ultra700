/* Simple blocking PCM sample player (mono 8kHz 16-bit) for MT6261 using AFE sine test path.
 * This is a placeholder that writes samples via the existing Beep() style generator by
 * directly toggling AFE_DAC_TEST; for real quality implement DMA to AFE FIFO. */
#ifndef PCM_PLAYER_H
#define PCM_PLAYER_H
#include <stdint.h>

#include "systemconfig.h"

void PCM_Player_Init(void);
void PCM_Player_PlaySample(void);                // plays built‑in demo
void PCM_Player_PlayBuffer(const int16_t *buf, uint32_t samples, uint32_t sample_rate);

#endif /* PCM_PLAYER_H */
