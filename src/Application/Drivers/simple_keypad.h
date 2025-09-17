/*
* Simple Keypad Scanner for MT6261 DZ09
*/
#ifndef _KEYPAD_SCANNER_H_
#define _KEYPAD_SCANNER_H_

#include "systemconfig.h"
#include "lrtimer.h"

// Support up to three 5x5 keypad banks (bits 0..4 / 5..9 / 10..14) plus optional bit15 per register
#define KEYPAD_ROWS     5
#define KEYPAD_COLS     5
#define KEYPAD_BANKS    3
// Base matrix keys (3 banks * 25) = 75; we also expose bit15 of each KP_MEM as 5 virtual keys
#define KEYPAD_EXTRA_KEYS 5
#define KEYPAD_MAX_KEYS (KEYPAD_ROWS * KEYPAD_COLS * KEYPAD_BANKS + KEYPAD_EXTRA_KEYS)

typedef enum {
    KEY_RELEASED = 0,
    KEY_PRESSED = 1
} TKEY_STATE;

typedef struct {
    uint8_t bank;      // logical bank (0..2, 3 for virtual extra keys)
    uint8_t key_id;    // linear key index (0..KEYPAD_MAX_KEYS-1)
    TKEY_STATE state;  // press/release
    boolean valid;     // event state valid flag
} TKEY_EVENT;

boolean Keypad_Initialize(void);
void Keypad_StartScanning(void);
boolean Keypad_GetKeyEvent(TKEY_EVENT *event);
const char* Keypad_GetKeyName(uint8_t key_id);

// Debug/learn helpers to discover wiring without pinout
void Keypad_DumpRaw(void);                 // Print KP_MEM1..5 raw bitmaps once
void Keypad_SetLearnMode(boolean enable);  // When enabled, logs every key press/release with raw states
void Keypad_SetVisualDebug(boolean enable); // Enable/disable on-screen raw matrix overlay

#endif
