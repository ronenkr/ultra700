/*
* Simple Delay Functions Implementation
*/
#include "systemconfig.h"
#include "delay.h"
#include "ustimer.h"

void delay_cycles(uint32_t cycles)
{
    if (cycles == 0) return;

    // Prefer hardware timer for accuracy: convert cycles to microseconds first
    uint32_t us = cycles / CYCLES_PER_US;
    if (us)
    {
        // Ensure USC is running and pause for computed microseconds
        USC_StartCounter();
        USC_Pause_us(us);
    }

    // Handle any sub-microsecond remainder with a tiny busy loop (approximate)
    // This keeps behavior reasonable without relying on inline asm.
    uint32_t rem = cycles % CYCLES_PER_US;
    if (rem)
    {
        volatile uint32_t n = rem / 3u + 1u; // ~3 cycles/iter rough estimate
        while (n--) { /* spin */ }
    }
}

void delay_us(uint32_t microseconds)
{
    if (microseconds == 0) return;

    // Use hardware 1 MHz microsecond counter for precise delays
    USC_StartCounter();
    USC_Pause_us(microseconds);
}

void delay_ms(uint32_t milliseconds)
{
    if (milliseconds == 0) return;
    
    // For longer delays, break into smaller chunks to avoid overflow
    while (milliseconds > 0)
    {
        uint32_t chunk = (milliseconds > 1000) ? 1000 : milliseconds;
        delay_us(chunk * 1000);  // Convert to microseconds
        milliseconds -= chunk;
    }
}

void delayMs(uint32_t milliseconds)
{
    // Alias for delay_ms for compatibility
    delay_ms(milliseconds);
}

// Test function to verify delay accuracy
void delay_test(void)
{
    DebugPrint("Delay Test: Starting delay verification\n");
    
    // Test microsecond delays
    DebugPrint("Delay Test: 100us delay...\n");
    delay_us(100);
    DebugPrint("Delay Test: 100us complete\n");
    
    // Test millisecond delays
    DebugPrint("Delay Test: 10ms delay...\n");
    delay_ms(10);
    DebugPrint("Delay Test: 10ms complete\n");
    
    // Test longer delay
    DebugPrint("Delay Test: 250ms delay...\n");
    delayMs(250);
    DebugPrint("Delay Test: 250ms complete\n");
    
    DebugPrint("Delay Test: All delay tests complete\n");
}
