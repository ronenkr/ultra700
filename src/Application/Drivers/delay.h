/*
* Simple Delay Functions for MT6261 @ 260MHz
*/
#ifndef _DELAY_H_
#define _DELAY_H_

#include "systemconfig.h"

// CPU frequency is 260MHz
#define CPU_FREQ_HZ     260000000UL
#define CYCLES_PER_US   (CPU_FREQ_HZ / 1000000UL)  // 260 cycles per microsecond
#define CYCLES_PER_MS   (CPU_FREQ_HZ / 1000UL)     // 260000 cycles per millisecond

// Basic delay functions
void delay_cycles(uint32_t cycles);
void delay_us(uint32_t microseconds);
void delay_ms(uint32_t milliseconds);
void delayMs(uint32_t milliseconds);  // Alias for compatibility

// Inline assembly delay for precise timing
static inline void delay_cycles_inline(uint32_t cycles)
{
    // Simple loop-based delay
    // Each loop iteration takes approximately 4 cycles (load, compare, branch, decrement)
    /*volatile uint32_t count = cycles /= 4;

    while(count>0){
        count--;
    }*/
}

#endif // _DELAY_H_
