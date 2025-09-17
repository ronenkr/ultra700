/*
* Simple Keypad Scanner Implementation (MT6261 KP controller)
*/
#include "systemconfig.h"
#include "simple_keypad.h"
#include "mt6261.h"   // For KP_BASE definition
#include "kp.h"
#include "gpio.h"
#include "ili9341.h"
#include "ustimer.h"
#include "usb_print.h"

// Dedicated KP pins mapping (ROW: GPIO21..17 = KROW0..4, COL: GPIO16..12 = KCOL0..4)
static const uint32_t rows[KEYPAD_ROWS] = {GPIO21, GPIO20, GPIO19, GPIO18, GPIO17};
static const uint32_t cols[KEYPAD_COLS] = {GPIO16, GPIO15, GPIO14, GPIO13, GPIO12};

// Key names for three banks (suffix _2, _3)
static const char* key_names[KEYPAD_MAX_KEYS] = {
    // Bank 0
    "CTL", "SHT", "3", "ENT", "V",
    "4", "DEL", "G", "C", "P", 
    "T", "8", "FM", "LFT", "F",
    "0", "0", "$", "C", "H",
    "L", "F", "LF", "O", "R",
    // Bank 1
    "1_2", "RU", "LU", "A_2", "&",
    "4_2", "M", "X", "C_2", "K", 
    "D", "8_2", "I", "E", "F_2",
    "RGT", "CAL", "#_2", "SPC", "ALT",
    "N", "A", "LF_2", "J", "Q",
    // Bank 2
    "1_3", "U", "MUS", "A_3", "UP",
    "4_3", "5_3", "B", "Z", "D_3", 
    "S", "8_3", "Y", "W", "F_3",
    "DWN", "0_3", "#_3", "OK", "H_3",
    "UP_3", "DN_3", "LF_3", "RT_3", "SEL_3",
    // Extra bit15 virtual keys (one per KP_MEM register)
    "SYM", "OTH", "X3", "H", "X5"
};

// Simple state tracking
static boolean initialized = false;
static boolean scanning = false;
static pTIMER scan_timer = NULL;
static TKEY_EVENT current_event;
static boolean event_pending = false;
// Track last state per key to emit transitions
static boolean last_state[KEYPAD_MAX_KEYS] = {0};
// Optional idle baseline (currently disabled for full visibility of all keys)
static uint16_t idle_mem[KEYPAD_COLS] = {0x1F, 0x1F, 0x1F, 0x1F, 0x1F};
static boolean idle_valid = false;
static boolean use_baseline_filter = false; // set true if you want to suppress baseline-stuck bits

// GPIO probe state (for discovering a second 5x5 matrix on general GPIOs)
static void Keypad_TimerHandler(pTIMER Timer);
static void Keypad_Scan(void);
static void Keypad_LogRaw(const uint16_t mem[KEYPAD_COLS]);
static void Keypad_DrawOverlay(const uint16_t mem_bank[KEYPAD_BANKS][KEYPAD_COLS]);
// New: draw all 80 potential KP bits (5 registers * 16 bits) in a 5x16 grid
static void Keypad_DrawAllBits(const uint16_t raw_regs[KEYPAD_COLS]);

// Learn mode outputs detailed logs of raw KP_MEM and decoded keys
static boolean learn_mode = true;
static boolean visual_debug = true;

boolean Keypad_Initialize(void)
{
    uint32_t i;
    
    if (initialized) return true;
    
    // Put dedicated pins into keypad mode (alternate function)
    for (i = 0; i < KEYPAD_ROWS; i++)
    {
        uint32_t mode = (i == 0) ? GPIO21_MODE_KROW0 :
                       (i == 1) ? GPIO20_MODE_KROW1 :
                       (i == 2) ? GPIO19_MODE_KROW2 :
                       (i == 3) ? GPIO18_MODE_KROW3 : GPIO17_MODE_KROW4;
        GPIO_Setup(rows[i], GPMODE(mode));
    }
    for (i = 0; i < KEYPAD_COLS; i++)
    {
        uint32_t mode = (i == 0) ? GPIO16_MODE_KCOL0 :
                       (i == 1) ? GPIO15_MODE_KCOL1 :
                       (i == 2) ? GPIO14_MODE_KCOL2 :
                       (i == 3) ? GPIO13_MODE_KCOL3 : GPIO12_MODE_KCOL4;
        GPIO_Setup(cols[i], GPMODE(mode));
    }

    // Configure KP controller
    // Debounce ~ reasonable value (tuned empirically); KP_DEB field is 14-bit
    KP_DEBOUNCE = KP_DEB(0x0100);
    // Scan timing: rows/cols count (0-based) and small intervals
    KP_SCAN_TIMING = KP_ROW_SCAN(KEYPAD_ROWS - 1) |
                     KP_COL_SCAN(KEYPAD_COLS - 1) |
                     KP_ROW_INTERVAL(2) |
                     KP_COL_INTERVAL(2);
    // Selection: triple mode exposes bits for 3 logical banks (0..14 per register)
    KP_SEL = KP_SEL_TRIPLE |
             KP_SAMPLE(3) |
             KP_COL0 | KP_COL1 | KP_COL2 | KP_COL3 | KP_COL4;
    // Enable keypad engine
    KP_EN = KP_ENABLE;

    // Prime idle baseline after first clear scan (only if filtering enabled)
    idle_valid = false;

    // Create timer (scan every 50ms to be gentle)
    scan_timer = LRT_Create(50, Keypad_TimerHandler, TF_AUTOREPEAT);
    if (!scan_timer) return false;
    
    initialized = true;
    USB_Print("Simple Keypad: Initialized (HW KP)\n");
    return true;
}

void Keypad_StartScanning(void)
{
    if (!initialized || scanning) return;
    
    scanning = true;
    LRT_Start(scan_timer);
    USB_Print("Simple Keypad: Started scanning\n");
}

static void Keypad_TimerHandler(pTIMER Timer)
{
    Keypad_Scan();
}

static void Keypad_Scan(void)
{
    // Linear decoding: bits 0..14 across each KP_MEM register build 75 matrix keys.
    // We also surface bit15 from each register as 5 extra keys at indices 75..79.
    uint16_t raw[KEYPAD_COLS];
    (void)KP_STATUS; // clear edge/status latch
    raw[0] = KP_MEM1;
    raw[1] = KP_MEM2;
    raw[2] = KP_MEM3;
    raw[3] = KP_MEM4;
    raw[4] = KP_MEM5;

    if (visual_debug) {
        Keypad_DrawAllBits(raw);
    }

    // Optional baseline capture (only if enabled)
    if (use_baseline_filter && !idle_valid) {
        boolean any_pressed = false;
        for (uint32_t r=0;r<KEYPAD_COLS;r++) {
            if ((raw[r] & 0x7FFF) != 0x7FFF) { any_pressed = true; break; }
        }
        if (!any_pressed) { for (uint32_t i=0;i<KEYPAD_COLS;i++) idle_mem[i] = raw[i]; idle_valid = true; }
    }

    for (uint32_t bank=0; bank<KEYPAD_BANKS; bank++) {
        for (uint32_t row=0; row<KEYPAD_ROWS; row++) {
            for (uint32_t col=0; col<KEYPAD_COLS; col++) {
                uint32_t linear = bank*25 + row*5 + col; // 0..74
                uint32_t reg = linear / 15;               // 0..4
                uint32_t bit = linear % 15;               // 0..14
                uint16_t mask = (uint16_t)(1u << bit);
                boolean raw_pressed = ((raw[reg] & mask) == 0u); // active low
                if (use_baseline_filter && idle_valid) {
                    boolean base_pressed = ((idle_mem[reg] & mask) == 0u);
                    if (raw_pressed == base_pressed) raw_pressed = false; // suppress
                }
                uint8_t key_index = (uint8_t)linear;
                if (raw_pressed != last_state[key_index]) {
                    last_state[key_index] = raw_pressed;
                    if (!event_pending) {
                        current_event.bank = (uint8_t)bank;
                        current_event.key_id = key_index;
                        current_event.state = raw_pressed ? KEY_PRESSED : KEY_RELEASED;
                        current_event.valid = true;
                        event_pending = true;
                        if (raw_pressed) {
                            //USB_Print("KP EVT bank=%u col=%u row=%u key=%u reg=%u bit=%u raw=0x%04X\n", (unsigned)bank, (unsigned)col, (unsigned)row, (unsigned)key_index, (unsigned)reg, (unsigned)bit, (unsigned)raw[reg]);
                        }
                        return;
                    }
                }
            }
        }
    }

    // Process extra bit15 keys (virtual bank index = 3)
    for (uint32_t reg=0; reg<KEYPAD_COLS; reg++) {
        uint32_t bit = 15;
        uint16_t mask = (uint16_t)(1u << bit);
        boolean raw_pressed = ((raw[reg] & mask) == 0u);
        uint8_t key_index = (uint8_t)(KEYPAD_ROWS * KEYPAD_COLS * KEYPAD_BANKS + reg); // 75..79
        if (raw_pressed != last_state[key_index]) {
            last_state[key_index] = raw_pressed;
            if (!event_pending) {
                current_event.bank = 3; // virtual
                current_event.key_id = key_index;
                current_event.state = raw_pressed ? KEY_PRESSED : KEY_RELEASED;
                current_event.valid = true;
                event_pending = true;
                if (raw_pressed) {
                    //USB_Print("KP EVT bank=3 extra reg=%u bit=15 key=%u raw=0x%04X\n", (unsigned)reg, (unsigned)key_index, (unsigned)raw[reg]);
                }
                return;
            }
        }
    }
}

static void Keypad_DrawAllBits(const uint16_t raw_regs[KEYPAD_COLS])
{
    // Draw into overlay layer 3 (ARGB8888) if available
    if (!LCDScreen.VLayer[3].Initialized || !LCDScreen.VLayer[3].FrameBuffer) return;
    uint32_t *fb = (uint32_t*)LCDScreen.VLayer[3].FrameBuffer;
    
    const uint16_t cell = 6;      // cell size
    const uint16_t gap  = 1;
    // Colors in ARGB8888
    const uint32_t col_off    = 0xFF606060; // gray
    const uint32_t col_on     = 0xFF00FF00; // green
    const uint32_t col_unused = 0xFF303030; // darker gray
    const uint32_t transparent= 0x00000000; // fully transparent

    // Clear only the region we use (5 rows * (cell+gap) , 16 cols * (cell+gap))
    uint16_t used_w = 16 * (cell + gap);
    uint16_t used_h = 5  * (cell + gap);
    if (used_w > LCD_XRESOLUTION) used_w = LCD_XRESOLUTION;
    if (used_h > LCD_YRESOLUTION) used_h = LCD_YRESOLUTION;

    /* Center horizontally, place at bottom (with 1px margin) */
    uint16_t origin_x = 0;
    uint16_t origin_y = 0;
    if (LCD_XRESOLUTION > used_w) origin_x = (uint16_t)((LCD_XRESOLUTION - used_w) / 2);
    if (LCD_YRESOLUTION > used_h) {
        uint16_t margin = 1; /* bottom margin */
        if (used_h + margin <= LCD_YRESOLUTION) origin_y = (uint16_t)(LCD_YRESOLUTION - used_h - margin);
    }

    for (uint16_t yy=0; yy<used_h; yy++) {
        uint32_t *row = fb + (origin_y + yy) * LCD_XRESOLUTION + origin_x;
        for (uint16_t xx=0; xx<used_w; xx++) row[xx] = transparent;
    }

    for (uint32_t reg = 0; reg < KEYPAD_COLS; reg++) {
        uint16_t reg_val = raw_regs[reg];
        for (uint32_t bit = 0; bit < 16; bit++) {
            boolean pressed = ((reg_val & (1u << bit)) == 0u);
            boolean plausible = (bit < 15);
            uint32_t color = pressed ? col_on : (plausible ? col_off : col_unused);
            uint16_t x = origin_x + bit * (cell + gap);
            uint16_t y = origin_y + reg * (cell + gap);
            if (x + cell >= LCD_XRESOLUTION || y + cell >= LCD_YRESOLUTION) continue;
            for (uint16_t dy = 0; dy < cell; dy++) {
                uint32_t *dst = fb + (y + dy) * LCD_XRESOLUTION + x;
                for (uint16_t dx = 0; dx < cell; dx++) dst[dx] = color;
            }
        }
    }
    // Update just the overlay area
    TRECT upd = { (int16_t)origin_x, (int16_t)origin_y, (int16_t)(origin_x + used_w - 1), (int16_t)(origin_y + used_h - 1) };
    LCDIF_UpdateRectangle(upd);
}

static void Keypad_LogRaw(const uint16_t mem[KEYPAD_COLS])
{
    //USB_Print("KP RAW: C0=%02X C1=%02X C2=%02X C3=%02X C4=%02X (0=pressed)\n",(mem[0] & 0x1F), (mem[1] & 0x1F), (mem[2] & 0x1F), (mem[3] & 0x1F), (mem[4] & 0x1F));
}

void Keypad_DumpRaw(void)
{
    uint16_t mem[KEYPAD_COLS];
    (void)KP_STATUS;
    mem[0] = KP_MEM1;
    mem[1] = KP_MEM2;
    mem[2] = KP_MEM3;
    mem[3] = KP_MEM4;
    mem[4] = KP_MEM5;
    Keypad_LogRaw(mem);
}

void Keypad_SetLearnMode(boolean enable)
{
    learn_mode = enable;
    USB_Print("Keypad learn mode: %s\n", enable ? "ON" : "OFF");
}

void Keypad_SetVisualDebug(boolean enable)
{
    visual_debug = enable;
}

static void Keypad_DrawOverlay(const uint16_t mem_bank[KEYPAD_BANKS][KEYPAD_COLS])
{
    const uint16_t origin_x0 = 2;
    const uint16_t origin_y0 = 2;
    const uint16_t cell = 9;
    const uint16_t gap = 1;
    const uint16_t bank_gap = 6;
    const uint16_t on  = 0x07E0;
    const uint16_t on2 = 0x001F;
    const uint16_t off = 0xC618;
    for (uint32_t bank = 0; bank < KEYPAD_BANKS; bank++) {
        uint16_t bank_x_offset = origin_x0 + bank * (KEYPAD_COLS * (cell + gap) + bank_gap);
        for (uint32_t col = 0; col < KEYPAD_COLS; col++) {
            uint16_t bits = mem_bank[bank][col] & 0x001F;
            for (uint32_t row = 0; row < KEYPAD_ROWS; row++) {
                boolean pressed = ((bits & (1u << row)) == 0u);
                uint16_t x = bank_x_offset + col * (cell + gap);
                uint16_t y = origin_y0 + row * (cell + gap);
                //ILI9341_DrawFilledRect(x, y, cell, cell, pressed ? (bank ? on2 : on) : off);
            }
        }
    }
}


boolean Keypad_GetKeyEvent(TKEY_EVENT *event)
{
    if (!event || !event_pending) return false;
    
    *event = current_event;
    event_pending = false;
    return true;
}

const char* Keypad_GetKeyName(uint8_t key_id)
{
    return (key_id < KEYPAD_MAX_KEYS) ? key_names[key_id] : "UNKNOWN";
}
