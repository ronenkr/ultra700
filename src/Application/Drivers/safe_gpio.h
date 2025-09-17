/*
* Safe GPIO List for MT6261 Testing
*/
#ifndef _SAFE_GPIO_H_
#define _SAFE_GPIO_H_

#include "systemconfig.h"

// Safe GPIOs for testing (avoid critical system functions)
typedef struct {
    uint32_t gpio_pin;
    const char* description;
    boolean is_safe;
} TSAFE_GPIO;

extern const TSAFE_GPIO safe_gpio_list[];
extern const uint32_t safe_gpio_count;

void SafeGPIO_Initialize(void);
void SafeGPIO_TestAll(void);
void SafeGPIO_TestSingle(uint32_t gpio_index);
void SafeGPIO_ListAll(void);

#endif
