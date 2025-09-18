/*
* Dual LED Controller Implementation
*/
#include "systemconfig.h"
#include "mt6261.h"
#include "single_led.h"
#include "delay.h"

#define FLASH_LED_GPIO   GPIO5  // Flash LED (confirmed)
#define PHONE_LED_GPIO   GPIO7  // Red Phone LED (confirmed)
#define BLUE_LED_GPIO    GPIO3  // Blue LED (new)

static boolean flash_led_state = false;
static boolean phone_led_state = false;
static boolean blue_led_state  = false;

void SingleLED_Initialize(void)
{
    // Setup GPIO5 as output (Flash LED)
    GPIO_Setup(FLASH_LED_GPIO, GPDO | GPMODE(GPIO05_MODE_IO));
    GPIO_DATAOUT(FLASH_LED_GPIO, false);
    
    // Setup GPIO7 as output (Phone LED) 
    GPIO_Setup(PHONE_LED_GPIO, GPDO | GPMODE(GPIO07_MODE_IO));
    GPIO_DATAOUT(PHONE_LED_GPIO, false);

    // Setup GPIO3 as output (Blue LED)
    GPIO_Setup(BLUE_LED_GPIO, GPDO | GPMODE(GPIO03_MODE_IO));
    GPIO_DATAOUT(BLUE_LED_GPIO, false);
    
    DebugPrint("LEDs: Init GPIO5(Flash) GPIO7(Phone) GPIO3(Blue)\n");
}

void SingleLED_Set(TLED_TYPE led, boolean state)
{
    if (led == LED_FLASH) {
        flash_led_state = state;
        GPIO_DATAOUT(FLASH_LED_GPIO, state);
        DebugPrint("Flash LED: %s\n", state ? "ON" : "OFF");
    } else if (led == LED_PHONE) {
        phone_led_state = state;
        GPIO_DATAOUT(PHONE_LED_GPIO, state);
        DebugPrint("Phone LED: %s\n", state ? "ON" : "OFF");
    } else if (led == LED_BLUE) {
        blue_led_state = state;
        GPIO_DATAOUT(BLUE_LED_GPIO, state);
        DebugPrint("Blue LED: %s\n", state ? "ON" : "OFF");
    }
}

void SingleLED_Toggle(TLED_TYPE led)
{
    if (led == LED_FLASH) {
        SingleLED_Set(LED_FLASH, !flash_led_state);
    } else if (led == LED_PHONE) {
        SingleLED_Set(LED_PHONE, !phone_led_state);
    } else if (led == LED_BLUE) {
        SingleLED_Set(LED_BLUE, !blue_led_state);
    }
}

void SingleLED_Flash_Pattern(void)
{
    // Alternating flash pattern
    for (int i = 0; i < 3; i++)
    {
        // Flash LED blink
        SingleLED_Set(LED_FLASH, true);
        delayMs(150); // 150ms
        SingleLED_Set(LED_FLASH, false);
        delayMs(150);
        
        // Phone LED blink  
        SingleLED_Set(LED_PHONE, true);
        delayMs(150);
        SingleLED_Set(LED_PHONE, false);
        delayMs(150);

        // Blue LED blink
        SingleLED_Set(LED_BLUE, true);
        delayMs(150);
        SingleLED_Set(LED_BLUE, false);
        delayMs(150);
    }
    
    DebugPrint("LED Flash Pattern Complete\n");
}
