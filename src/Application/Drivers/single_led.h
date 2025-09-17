/*
* Dual LED Controller (Flash + Phone LEDs)
*/
#ifndef _SINGLE_LED_H_
#define _SINGLE_LED_H_

#include "systemconfig.h"
#include "gpio.h"

typedef enum {
    LED_FLASH = 0,  // GPIO5 - Flash LED
    LED_PHONE = 1,  // GPIO7 - Red Phone LED
    LED_BLUE  = 2   // GPIO4 - Blue LED (newly discovered)
} TLED_TYPE;

void SingleLED_Initialize(void);
void SingleLED_Toggle(TLED_TYPE led);
void SingleLED_Set(TLED_TYPE led, boolean state);
void SingleLED_Flash_Pattern(void);

#endif
