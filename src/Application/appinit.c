// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/*
* This file is part of the DZ09 project.
*
* Copyright (C) 2020, 2019 AJScorp
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; version 2 of the License.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
* General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*/
#include "systemconfig.h"
#include "appinit.h"
#include <stdint.h>
#include "pmu.h"  // For VMC_CON0, PMU_SetVoltageVMC, etc.
#include "appdrivers.h"
#include "simple_keypad.h"
#include "single_led.h"
#include "safe_gpio.h"
#include "gditypes.h"
#include "gdifont.h"      // For text drawing types/APIs
#include "fontlib.h"      // For fontCourier20h
#include "usbdevice_cdc.h"
#include "i2c_scanner.h"
#include "gpio.h"          // For GPIO access
#include "ili9341.h"       // For drawing (if needed)
#include "mt6261.h"        // For GPIO_BASE definition
#include "lrtimer.h"       // For 200ms timer
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#ifndef UINT32_MAX
// Fallback if stdint wasn't provided by toolchain headers for some reason
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef signed char int8_t;
typedef signed short int16_t;
typedef signed int int32_t;
#endif
#include "usb_print.h"
#include "audio_note.h"   // For tone playback on key press
#include "pcm_player.h"    // For PCM sample playback on 'E'
#if 0 // legacy SD card driver header (not used in minimal path)
#include "sdcard.h"
#endif
#include "msdc.h"          // For minimal raw SD access
#include "sdcmd.h"
#include "Drivers/fs_fat32.h" // FAT32 minimal implementation
// Root clock (CONFIG_BASE) minimal defs
#ifndef CONFIG_BASE
#define CONFIG_BASE 0xA0010000u
#endif
#define PLL_CLK_CONDD   (*(volatile uint32_t*)(CONFIG_BASE + 0x010C))
#ifndef RG_MSDC1_26M_SEL
#define RG_MSDC1_26M_SEL (1u<<0)
#define RG_MSDC2_26M_SEL (1u<<4)
#endif
static void msdc_root_clock_enable(int ctrl_index){
    uint32_t bit = (ctrl_index==0)? RG_MSDC1_26M_SEL : RG_MSDC2_26M_SEL;
    uint32_t before = PLL_CLK_CONDD;
    PLL_CLK_CONDD = before | bit;
    uint32_t after = PLL_CLK_CONDD;
    USB_Print("SD(min): rootclk ctrl=%d bit=0x%lX %08lX->%08lX\r\n",ctrl_index,(unsigned long)bit,(unsigned long)before,(unsigned long)after);
}

// FAT32 directory listing callback (file-scope) used by 'p' key
static void fat32_list_print_cb(const char *name83, uint8_t attr, uint32_t firstCluster, uint32_t sizeBytes, void *user)
{
    (void)user;
    char type = (attr & 0x10) ? 'D' : 'F';
    USB_Printf(" %c %s %lu %08lX\r\n", type, name83, (unsigned long)sizeBytes, (unsigned long)firstCluster);
}
// Watchdog handling: if project provides WDT APIs, define WDT_PET() macro accordingly.
#ifndef WDT_PET
#define WDT_PET() do { /* no-op */ } while(0)
#endif

// ---------------- Integer Benchmark (triggered by 'W' key, key_id 19) ----------------
static void RunIntBenchmark(void)
{
    const uint32_t outer_loops = 2000;      // adjust to tune runtime
    const uint32_t inner_unroll = 32;       // arithmetic ops per loop
    volatile uint32_t a = 0x12345678u;
    volatile uint32_t b = 0x9E3779B1u;
    volatile uint32_t c = 0x0FEDCBA9u;
    uint32_t checksum = 0;
    USC_StartCounter();
    uint32_t start = USC_GetCurrentTicks();
    for (uint32_t i=0;i<outer_loops;i++) {
        for (uint32_t j=0;j<inner_unroll;j++) {
            a += (b ^ (c >> 3)) + (i * 1103515245u + j);
            b = (b << 7) | (b >> 25);
            c ^= (a + (b * 3u));
            a = (a ^ (c >> 5)) + (b ^ 0xA5A5A5A5u);
            b += (c | 0x55AA55AAu) ^ (a >> 9);
            c = (c + (a * 1664525u) + 1013904223u);
            if ((j & 7u) == 0u) {
                uint32_t d = (a ^ b ^ c) | 1u;
                a ^= (c / d);
                b ^= (c % d);
            }
        }
        checksum ^= (a ^ b ^ c);
    }
    uint32_t end = USC_GetCurrentTicks();
    uint32_t us = (end >= start) ? (end - start) : 0u; // 1 tick = 1us (USC_FREQUENCY=1MHz)
    uint32_t iters = outer_loops * inner_unroll;
    uint32_t approx_ops = iters * 10u; // rough ops count
    uint32_t ops_per_us = us ? (approx_ops / us) : 0u;
    USB_Print("INT-BENCH: time=%luus loops=%lu inner=%lu iters=%lu ops≈%lu ops/us=%lu checksum=0x%08lX\n",
              (unsigned long)us,
              (unsigned long)outer_loops,
              (unsigned long)inner_unroll,
              (unsigned long)iters,
              (unsigned long)approx_ops,
              (unsigned long)ops_per_us,
              (unsigned long)checksum);
}
// ------------------------------------------------------------------------------

// Simple on-screen debug text when 'W' key (internal id 63) is pressed
static void DrawText_WPressed(void)
{
    // Choose layer 0 (RGB565) for now; overlay layer 3 is used for keypad bits and may be cleared.

    TVLINDEX layer = 0;
    static char msg[] = "W PRESSED"; // message (modifiable storage)
    // Build text object
    TTEXT txt = Text(&fontCourier20h, msg, (TTXTALIGN)(AH_LEFT | AV_TOP), TextColor(clWhite, clBlack));
    // Decide top-left position (small margin)
    int16_t x = 4;
    int16_t y = 4;
    // Compute client rectangle sized to text
    TRECT client = { x, y, (int16_t)(x + txt.Extent.sx - 1), (int16_t)(y + txt.Extent.sy - 1) };
    // Clear background first to avoid residual glyph noise
    GDI_FillRectangle(layer, client, clBlack);
    // Clip equals client (no scrolling needed)
    TRECT clip = client;
    // Render text (foreground white, background black)
    pRLIST back = GDI_DrawText(layer, &txt, &client, &clip, txt.Color.ForeColor, txt.Color.BackColor);
    // If library returned additional rectangles (background areas) fill them black too (mirrors label code usage)
    if (back) {
        for (uint32_t i = 0; i < back->Count; i++) {
            GDI_FillRectangle(layer, back->Item[i], clBlack);
        }
        GDI_DeleteRList(back);
    }
    // Push updated region to LCD
    LCDIF_UpdateRectangle(client);
    USB_Print("Displayed text: '%s' at (%d,%d) size %dx%d\n", msg, (int)x, (int)y, txt.Extent.sx, txt.Extent.sy);
}

// Minimal SD init + read LBA0 using MSDC0
// Wait for command ready (CMDRDY) with a simple timeout in microseconds
static boolean msdc_wait_cmd_ready(volatile TMSDCREGS *m, uint32_t tout_us)
{
    boolean saw_busy = false;
    while (tout_us--) {
        uint32_t cmdsta = m->SDC_CMDSTA;
        uint32_t sdcsta = m->SDC_STA; // busy bits
        if (cmdsta & SDC_CMDRDY) return true;      // normal completion
        if (cmdsta & SDC_CMDTO) return false;      // explicit timeout from HW
        if (sdcsta & (SDC_BECMDBUSY | SDC_FECMDBUSY)) saw_busy = true; // command engine toggled
        else if (saw_busy) {
            // Busy cleared after being set -> treat as completion even if CMDRDY not set
            return true;
        }
        USC_Pause_us(1);
    }
    // Fallback: success if CMDRDY or busy never asserted (card might be absent) -> report false
    return (m->SDC_CMDSTA & SDC_CMDRDY) != 0;
}

// Define to enable verbose debug of minimal SD path
// Set to 1 for verbose logging during minimal SD init
#ifndef SD_MIN_DEBUG
#define SD_MIN_DEBUG 1
#endif


// Minimal wrapper using clean-room sd_minimal driver (MSDC2 only)
#include "sd_minimal.h"
static boolean SD_Minimal_InitAndReadLBA0(uint8_t *buf512)
{
    if (!buf512) return false;
    static boolean inited = false;
    if (!inited) {
        USB_Print("SD(min): init starting\n");
        if (!SDM_Init()) {
            const char *stage = SDM_GetLastFailStage();
            USB_Print("SD(min): init failed (stage=%s)\n", stage?stage:"?");
            return false;
        }
        inited = true;
        int ct = SDM_GetCardType();
        USB_Print("SD(min): card type=%d (0 none,1 SDSC,2 SDHC)\n", ct);
    }
    if (!SDM_ReadBlock0(buf512)) {
        USB_Print("SD(min): read LBA0 failed\n");
        return false;
    }
    USB_Print("SD(min): LBA0 first 16 bytes: ");
    for (int i=0;i<16;i++) USB_Print("%02X", buf512[i]);
    USB_Print("\r\n");
    return true;
}


// Simple USB CDC support for key event transmission
static TCDCEVENTER g_cdcEventer; // Persist for writes
static volatile boolean g_cdcConnected = false;

// --- Minimal inline GPIO monitor (GPIO0..11 excluding assigned 4,5,7,10,11) ---
// --- Binary counter on GPIO0..GPIO10 (11 bits) updating every 200ms ---
static pTIMER gpio_counter_timer = NULL;
static void GPIO_CounterTick(pTIMER t)
{
    (void)t;
    static uint16_t value = 0; // 11-bit counter
    uint16_t v = value;
    for (uint32_t pin = 0; pin <= 10; pin++) {
        GPIO_DATAOUT(pin, (v >> pin) & 0x1);
    }
                        
    value = (uint16_t)((value + 1) & 0x07FF); // wrap at 2^11
}
static void GPIO_CounterInit(void)
{
    // Configure pins 0..10 as outputs
    for (uint32_t pin = 0; pin <= 10; pin++) {
        GPIO_SETDIROUT(pin);
    }
    gpio_counter_timer = LRT_Create(200, GPIO_CounterTick, TF_AUTOREPEAT); // 200ms period
    if (gpio_counter_timer) LRT_Start(gpio_counter_timer);
    USB_Print("GPIO binary counter active on GPIO0..GPIO10 (200ms)\n");
}
// -------------------------------------------------------------------------

static void CDC_StatusChangeHandler(TCDCSTATUS status)
{
    if (status == CDC_CONNECTED) g_cdcConnected = true;
    else if (status == CDC_DISCONNECTED) g_cdcConnected = false;
}

static void CDC_DataReceivedHandler(uint32_t received)
{
    (void)received; // Not used yet
}

static void CDC_DataTransmittedHandler(uint32_t transmitted)
{
    (void)transmitted; // Not used yet
}

// Public USB print functions (declared in usb_print.h)
void USB_Print(const char *fmt, ...)
{
    if (!g_cdcConnected || fmt == NULL) return;
    char buf[192];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n <= 0) return;
    if (n >= (int)sizeof(buf)) n = (int)sizeof(buf) - 1;
    USB_CDC_Write(&g_cdcEventer, (uint8_t*)buf, (uint32_t)n);
}

void USB_Printf(const char *fmt, ...)
{
    // Simple alias; keep separate to minimize churn in other files if both names used
    if (!g_cdcConnected || fmt == NULL) return;
    char buf[192];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n <= 0) return;
    if (n >= (int)sizeof(buf)) n = (int)sizeof(buf) - 1;
    USB_CDC_Write(&g_cdcEventer, (uint8_t*)buf, (uint32_t)n);
}

boolean APP_Initialize(void)
{
    do
    {
        if (!GUI_Initialize()) break;

        BL_TurnOn(true); // Turn on backlight (also restarts timer)
        TRECT full = (TRECT){0,0,LCD_XRESOLUTION-1, LCD_YRESOLUTION-1};
        // Ensure base GUI layer (layer 0) exists (SimpleDisplay_Initialize was removed)
    if (GUILayer[0] == NULL) {
            
            if (GUI_CreateLayer(0, full, CF_RGB565, 0xFF, clBlack) == NULL) {
                USB_Print("Layer0 create failed\r\n");
                break;
            }
            // Make it visible
            GUI_SetObjectVisibility(GUILayer[0], true);
            // Clear to black (or any background color)
            GDI_FillRectangle(0, full, clBlack);
            LCDIF_UpdateRectangle(full);
            USB_Print("Layer0 created & initialized\r\n");
        }

        // Create overlay layer (layer 3) for keypad debug overlay (ARGB8888 for easy alpha blending)
        if (GUILayer[3] == NULL) {
            // Use smaller height portion or full screen; here full screen
            if (GUI_CreateLayer(3, full, CF_ARGB8888, 0x80, 0x00000000) == NULL) {
                USB_Print("Layer3 create failed (overlay)\r\n");
            } else {
                GUI_SetObjectVisibility(GUILayer[3], true);
                // Clear overlay to transparent
                uint32_t *fb3 = (uint32_t*)LCDScreen.VLayer[3].FrameBuffer;
                if (fb3) {
                    uint32_t pixels = (uint32_t)(LCD_XRESOLUTION * LCD_YRESOLUTION);
                    for (uint32_t i=0;i<pixels;i++) fb3[i] = 0x00000000; // fully transparent
                }
                LCDIF_UpdateRectangle(full);
                USB_Print("Layer3 (overlay) created & cleared\r\n");
            }
        }

        // Initialize simple keypad
        if (!Keypad_Initialize())
        {
            USB_Print("Simple Keypad: Failed to initialize\n");
            break;
        }

        // Start keypad scanning
        Keypad_StartScanning();

        // Initialize single LED test (safe)
        SingleLED_Initialize();

        //GPIO_CounterInit(); // start binary counter

        //SingleLED_Toggle(LED_PHONE);
        //SingleLED_Flash_Pattern();
        // Initialize safe GPIO analysis
        //SafeGPIO_Initialize();
        //SafeGPIO_ListAll();

        // **TEST: Delay verification**
    USB_Print("\n=== Delay Test Phase ===\n");
        //delay_test();

        // **TEST: Additional display verification**
    USB_Print("\n=== Display Test Phase ===\n");
    USB_Print("Testing GDI functions with forced updates...\n");

        // Test large rectangles with different colors with delays
        TRECT big_test1 = {10, 180, 80, 230};   // Bottom left - cyan
        TRECT big_test2 = {100, 180, 170, 230}; // Bottom center - magenta
        TRECT big_test3 = {190, 180, 260, 230}; // Bottom right - yellow

        GDI_FillRectangle(0, big_test1, 0xFF0000); // RED
        LCDIF_UpdateRectangle(big_test1);

        GDI_FillRectangle(0, big_test2, 0xFF00FF); // Magenta
        LCDIF_UpdateRectangle(big_test2);

        GDI_FillRectangle(0, big_test3, 0xFFFF00); // Yellow
        LCDIF_UpdateRectangle(big_test3);
        delayMs(220); // Wait 220ms

        USB_Initialize();
        USB_EnableDevice();

        // Initialize CDC interface (optional write over USB when key 1 pressed)
        memset(&g_cdcEventer, 0, sizeof(g_cdcEventer));
        g_cdcEventer.OutBufferSize = 256; // request buffer
        g_cdcEventer.OnStatusChange = CDC_StatusChangeHandler;
        g_cdcEventer.OnDataReceived = CDC_DataReceivedHandler;
        g_cdcEventer.OnDataTransmitted = CDC_DataTransmittedHandler;
        if (USB_CDC_Initialize() != NULL)
        {
            USB_CDC_Open(&g_cdcEventer);
        }


    // Run I2C scan (touch panel bus) once USB is ready for output
    //I2C_Scanner_Init();

    USB_Print("DZ09 Simple Keypad Scanner Started!\n");
    USB_Print("Press keys to see them light up on white LCD background\n");
    USB_Print("Key presses reset backlight timer\n");
    USB_Print("=== Special Key Functions ===\n");
    USB_Print("Key '1': Test next safe GPIO (includes GPIO5,7 LEDs)\n");
    USB_Print("Key '2': Toggle Flash LED (GPIO5)\n");
    USB_Print("Key '3': LED Flash Pattern (both LEDs)\n");
    USB_Print("Key 'B': List all safe/unsafe GPIOs\n");
    USB_Print("Key '4': Test all very safe GPIOs\n");
    USB_Print("Other keys: Phone LED (GPIO7) toggles\n");
    USB_Print("============================\n");

        return true;
    }
    while(0);

    return false;
}


void APP_ProcessEvents(void)
{
    TKEY_EVENT event;
    static uint32_t led_toggle_counter = 0;
    static uint32_t gpio_test_trigger = 0;
    // Key->GPIO mapping: keys {44,58,32,18,4,57,45,31,17,20,34,48} -> GPIO {0..11}
    // Map index in array to GPIO number; we set direction on first use then toggle.
    static const uint8_t map_keys[12] = {44,58,32,18,4,57,45,31,17,20,34,48};
    static uint8_t map_inited = 0; // bit per GPIO configured
    if (!map_inited) {
        // lazy init here not needed; per-pin init occurs on first press
        map_inited = 1; // sentinel that array exists
    }

    if (Keypad_GetKeyEvent(&event))
    {
        if (event.state == KEY_PRESSED)
        {
            BL_RestartBacklightTimer(true);
            //SingleLED_Toggle(LED_PHONE);

            // Handle other key-specific actions
            switch (event.key_id)
            {
                default: {
                    // Generic path: check if this key belongs to mapping table
                    int gpio = -1;
                    for (int i=0;i<12;i++) if (map_keys[i] == event.key_id) { gpio = i; break; }
                    if (gpio >= 0) {
                        if (!(map_inited & (1u << (gpio+1)))) { // reuse map_inited bits beyond bit0
                            GPIO_SETDIROUT((uint32_t)gpio);
                            map_inited |= (uint8_t)(1u << (gpio+1));
                        }
                        // Read current and toggle
                        // Assuming GPIO_DATAOUT writes; need read macro (if not available, maintain shadow)
                        // If no read accessor, maintain a shadow state array.
                        static uint16_t shadow_state = 0; // bit per GPIO 0..15
                        shadow_state ^= (1u << gpio);
                        GPIO_DATAOUT((uint32_t)gpio, (shadow_state >> gpio) & 1u);
                        USB_Printf("GPIO%d toggled by key %d -> %d\r\n", gpio, event.key_id, (shadow_state >> gpio) & 1u);
                        break; // handled
                    }
                }
                case 0: //ctrl key
                    //SafeGPIO_TestSingle(gpio_test_trigger % 6);
                    break;
                case 1:// 0 key
                    //SingleLED_Toggle(LED_FLASH);
                    
                    USB_Print("Key 1: Flash LED toggled\r\n");

                    break;
                case 49: //Q key
                    //SingleLED_Toggle(LED_BLUE);
                    //SingleLED_Flash_Pattern();
                    break;
                case 38: // 'E' key
                    PCM_Player_PlaySample();
                    USB_Printf("Key e: cpu freq: %u Hz\r\n", GetCPUFrequency());
                    break;
                case 76: // OTH key -> integer benchmark
                    RunIntBenchmark();
                    break;
                case 63: // 'W' key -> draw text using GDI
                    DrawText_WPressed();
                    Beep();
                    break;
                case 9:{ //p key
                    extern boolean SDM_Init(void);
                    extern unsigned SDM_GetCapacityMB(void);
                    extern boolean SDM_ReadBlock(uint32_t lba, uint8_t *buf);
                    static FAT32_Volume vol;
                    if (!SDM_Init()) { USB_Print("SD init fail\r\n"); break; }
                    USB_Printf("SD capacity ~%u MB\r\n", SDM_GetCapacityMB());
                    if (!FAT32_Mount(&vol)) { USB_Print("FAT32 mount fail\r\n"); break; }
                    USB_Print("Root dir listing:\r\n");
                    FAT32_ListRoot(&vol, fat32_list_print_cb, NULL);
                    break;
                }

                case 3: { // Enter key: minimal SD test (no SECTOR_SIZE dependency)
                    uint8_t buf[512];
                    boolean ok = SD_Minimal_InitAndReadLBA0(buf);
                    USB_Print("SD(min): %s (first byte 0x%02X)\r\n", ok?"OK":"FAIL", (unsigned)buf[0]);
                    break; }
                case 46: { // 'A' key -> play a short melody (blocking)
                    struct Note { uint16_t f; uint16_t ms; };
                    static const struct Note song[] = {
                        {440,250},  // A4
                        {494,250},  // B4
                        {523,250},  // C5
                        {587,250},  // D5
                        {659,300},  // E5
                        {587,200},  // D5
                        {523,200},  // C5
                        {659,400},  // E5
                        {698,250},  // F5
                        {784,250},  // G5
                        {880,400},  // A5
                        {784,250},  // G5
                        {698,250},  // F5
                        {659,350},  // E5
                        {523,250},  // C5
                        {440,450}   // A4
                    };
                    for (unsigned i = 0; i < (sizeof(song)/sizeof(song[0])); i++) {
                        AudioNote_PlayTone(song[i].f, song[i].ms);
                        USC_Pause_us(20000); // 20ms gap
                    }
                    USB_Print("Melody (A key) finished\r\n");
                    break;
                }
            }

            USB_Printf("Key %d '%s' pressed (bank %d)\r\n", event.key_id, Keypad_GetKeyName(event.key_id), event.bank);

        }
        else if (event.state == KEY_RELEASED)
        {

        }
    }
    // (Binary counter handled by timer)
}
