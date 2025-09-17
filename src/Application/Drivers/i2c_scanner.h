#ifndef _I2C_SCANNER_H_
#define _I2C_SCANNER_H_

#include "systemconfig.h"
#include "appconfig.h"
// Hardware I2C scanner (GPIO43=SCL, GPIO44=SDA) @100kHz
void I2C_Scanner_Init(void);
// Scan 7-bit address space (0x03..0x77) and report ACKed addresses over USB
void I2C_Scanner_Run(void);

#endif
