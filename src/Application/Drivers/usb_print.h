// Central USB print interface implemented in appinit.c
#ifndef _USB_PRINT_H_
#define _USB_PRINT_H_

#include "systemconfig.h"

// Formatted USB CDC print (printf style). Silently drops output if not connected.
void USB_Print(const char *fmt, ...);
// Optional alias kept for compatibility; implemented as wrapper.
void USB_Printf(const char *fmt, ...);

#endif // _USB_PRINT_H_
