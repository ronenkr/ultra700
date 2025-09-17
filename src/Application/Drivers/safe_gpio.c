/*
* Safe GPIO List Implementation for MT6261
* Based on datasheet analysis and system requirements
*/
#include "systemconfig.h"
#include "safe_gpio.h"
#include "ustimer.h"

// Comprehensive safe GPIO analysis for MT6261
// NOTE: Updated: Only GPIO0..GPIO11 are now treated as safe test candidates.
const TSAFE_GPIO safe_gpio_list[] = {
    // VERY SAFE - General purpose pins with minimal risk
    {GPIO0,  "GPIO0 - General I/O, safest choice", true},
    {GPIO1,  "GPIO1 - General I/O, very safe", true},
    {GPIO2,  "GPIO2 - General I/O, safe", true},
    {GPIO4,  "GPIO4 - General I/O, safe", true},
    
    // KNOWN LED PINS - Confirmed working LEDs
    {GPIO5,  "GPIO5 - Flash LED (CONFIRMED)", true},
    {GPIO7,  "GPIO7 - Red Phone LED (CONFIRMED)", true},
    
    // MODERATELY SAFE (still within 0..11) - keep flagged safe
    {GPIO8,  "GPIO8 - General I/O (safe set 0..11)", true},
    {GPIO9,  "GPIO9 - General I/O (safe set 0..11)", true},
    {GPIO10, "GPIO10 - General I/O (safe set 0..11)", true},
    {GPIO11, "GPIO11 - General I/O (safe set 0..11)", true},

    // Previously considered usable but now OUTSIDE restricted safe range -> mark unsafe
    {GPIO23, "GPIO23 - Restricted (outside 0..11) now UNSAFE", false},
    {GPIO27, "GPIO27 - Restricted (outside 0..11) now UNSAFE", false},
    {GPIO28, "GPIO28 - Restricted (outside 0..11) now UNSAFE", false},
    {GPIO30, "GPIO30 - Restricted (outside 0..11) now UNSAFE", false},
    {GPIO31, "GPIO31 - Restricted (outside 0..11) now UNSAFE", false},
    {GPIO32, "GPIO32 - Restricted (outside 0..11) now UNSAFE", false},
    {GPIO33, "GPIO33 - Restricted (outside 0..11) now UNSAFE", false},
    {GPIO34, "GPIO34 - Restricted (outside 0..11) now UNSAFE", false},
    
    // AVOID - Critical system functions
    {GPIO3,  "GPIO3 - JTAG TDI, AVOID on production", false},
    {GPIO6,  "GPIO6 - BPI functions, AVOID", false},
    
    // AVOID - Keypad matrix (if using keypad scanner)
    {GPIO12, "GPIO12 - KCOL4, AVOID if using keypad", false},
    {GPIO13, "GPIO13 - KCOL3, AVOID if using keypad", false},
    {GPIO14, "GPIO14 - KCOL2, AVOID if using keypad", false},
    {GPIO15, "GPIO15 - KCOL1, AVOID if using keypad", false},
    {GPIO16, "GPIO16 - KCOL0, AVOID if using keypad", false},
    {GPIO17, "GPIO17 - KROW4, AVOID if using keypad", false},
    {GPIO18, "GPIO18 - KROW3, AVOID if using keypad", false},
    {GPIO19, "GPIO19 - KROW2, AVOID if using keypad", false},
    {GPIO20, "GPIO20 - KROW1, AVOID if using keypad", false},
    {GPIO21, "GPIO21 - KROW0, AVOID if using keypad", false},
    
    // AVOID - Clock and critical functions
    {GPIO22, "GPIO22 - RF functions, AVOID", false},
    {GPIO24, "GPIO24 - RF functions, AVOID", false},
    {GPIO25, "GPIO25 - Clock functions, AVOID", false},
    {GPIO26, "GPIO26 - Clock functions, AVOID", false},
    {GPIO29, "GPIO29 - Clock related, AVOID", false},
    {GPIO35, "GPIO35 - System clocks, AVOID", false},
    {GPIO36, "GPIO36 - System clocks, AVOID", false},
    
    // AVOID - Power and system critical
    {GPIO37, "GPIO37 - Power management, AVOID", false},
    {GPIO38, "GPIO38 - Power management, AVOID", false},
    {GPIO39, "GPIO39 - SIM clock, AVOID", false},
    {GPIO40, "GPIO40 - System functions, AVOID", false},
    {GPIO41, "GPIO41 - System clocks, AVOID", false},
    {GPIO42, "GPIO42 - SIM clock, AVOID", false},
    {GPIO43, "GPIO43 - System functions, AVOID", false},
    {GPIO44, "GPIO44 - System functions, AVOID", false},
    {GPIO45, "GPIO45 - System functions, AVOID", false},
    {GPIO46, "GPIO46 - System clocks, AVOID", false},
    {GPIO47, "GPIO47 - System functions, AVOID", false},
    {GPIO48, "GPIO48 - System functions, AVOID", false},
    {GPIO49, "GPIO49 - System clocks, AVOID", false},
    {GPIO50, "GPIO50 - System clocks, AVOID", false},
    {GPIO51, "GPIO51 - System functions, AVOID", false},
    {GPIO52, "GPIO52 - System functions, AVOID", false},
    {GPIO53, "GPIO53 - Critical system, AVOID", false},
    {GPIO54, "GPIO54 - System functions, AVOID", false},
    {GPIO55, "GPIO55 - System functions, AVOID", false}
};

const uint32_t safe_gpio_count = sizeof(safe_gpio_list) / sizeof(TSAFE_GPIO);

// Only test the safest GPIOs to avoid bootloops
static const uint32_t very_safe_indices[] = {0, 1, 2, 3, 4, 5}; // GPIO0,1,2,4,5,7
static const uint32_t very_safe_count = 6;

void SafeGPIO_Initialize(void)
{
    DebugPrint("=== MT6261 Safe GPIO Analysis ===\n");
    DebugPrint("Total GPIOs analyzed: %d\n", safe_gpio_count);
    
    uint32_t safe_count = 0;
    for (uint32_t i = 0; i < safe_gpio_count; i++)
    {
        if (safe_gpio_list[i].is_safe) safe_count++;
    }
    
    DebugPrint("Safe GPIOs for testing: %d\n", safe_count);
    DebugPrint("Very safe GPIOs (recommended): %d\n", very_safe_count);
    DebugPrint("=====================================\n");
}

void SafeGPIO_ListAll(void)
{
    DebugPrint("\n=== SAFE GPIO LIST ===\n");
    
    DebugPrint("\n--- CONFIRMED LED PINS ---\n");
    DebugPrint("GPIO5: Flash LED (CONFIRMED working)\n");
    DebugPrint("GPIO7: Red Phone LED (CONFIRMED working)\n");
    
    DebugPrint("\n--- VERY SAFE (Recommended for testing) ---\n");
    for (uint32_t i = 0; i < very_safe_count; i++)
    {
        const TSAFE_GPIO *gpio = &safe_gpio_list[very_safe_indices[i]];
        if (gpio->gpio_pin != GPIO5 && gpio->gpio_pin != GPIO7) // Skip LEDs, already listed
        {
            DebugPrint("GPIO%d: %s\n", gpio->gpio_pin, gpio->description);
        }
    }
    
    DebugPrint("\n--- MODERATELY SAFE (Check your hardware) ---\n");
    for (uint32_t i = 0; i < safe_gpio_count; i++)
    {
        if (safe_gpio_list[i].is_safe && 
            i != 0 && i != 1 && i != 2 && i != 3) // Skip very safe ones
        {
            DebugPrint("GPIO%d: %s\n", 
                      safe_gpio_list[i].gpio_pin, 
                      safe_gpio_list[i].description);
        }
    }
    
    DebugPrint("\n--- AVOID (Critical system functions) ---\n");
    for (uint32_t i = 0; i < safe_gpio_count; i++)
    {
        if (!safe_gpio_list[i].is_safe)
        {
            DebugPrint("GPIO%d: %s\n", 
                      safe_gpio_list[i].gpio_pin, 
                      safe_gpio_list[i].description);
        }
    }
    
    DebugPrint("\n======================\n");
}

void SafeGPIO_TestSingle(uint32_t gpio_index)
{
    if (gpio_index >= very_safe_count)
    {
        DebugPrint("SafeGPIO: Index %d out of very safe range (0-%d)\n", 
                   gpio_index, very_safe_count - 1);
        return;
    }
    
    const TSAFE_GPIO *gpio = &safe_gpio_list[very_safe_indices[gpio_index]];
    
    DebugPrint("Testing GPIO%d: %s\n", gpio->gpio_pin, gpio->description);
    
    // Setup as output
    GPIO_Setup(gpio->gpio_pin, GPDO | GPMODE(0)); // Mode 0 = GPIO
    
    // Test pattern: 5 blinks
    for (int i = 0; i < 5; i++)
    {
        GPIO_DATAOUT(gpio->gpio_pin, true);
        DebugPrint("GPIO%d: ON\n", gpio->gpio_pin);
        USC_Pause_us(200000); // 200ms
        
        GPIO_DATAOUT(gpio->gpio_pin, false);
        DebugPrint("GPIO%d: OFF\n", gpio->gpio_pin);
        USC_Pause_us(200000); // 200ms
    }
    
    DebugPrint("GPIO%d test complete\n", gpio->gpio_pin);
}

void SafeGPIO_TestAll(void)
{
    DebugPrint("\n=== Testing VERY SAFE GPIOs ===\n");
    DebugPrint("WARNING: Only testing GPIO0,1,2,4 to avoid bootloops\n");
    DebugPrint("Watch for LED activity on these pins\n\n");
    
    for (uint32_t i = 0; i < very_safe_count; i++)
    {
        SafeGPIO_TestSingle(i);
        USC_Pause_us(500000); // 500ms delay between tests
    }
    
    DebugPrint("=== Safe GPIO test complete ===\n");
    DebugPrint("If you saw LED activity, those GPIOs are working!\n");
    DebugPrint("If no LEDs, try different GPIOs from the MODERATELY SAFE list\n");
}
