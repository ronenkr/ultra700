#include "sd_minimal.h"
#include <string.h>
#include <stdio.h>
#include "usb_print.h"
#include "systemconfig.h" // pulls in base addresses, boolean typedef, etc.
#include "pmu.h"
#include "pctl.h"
#include "gpio.h"
#include "pll.h"
#include "ustimer.h"

#ifndef CONFIG_BASE
#define CONFIG_BASE 0xA0010000u
#endif
#ifndef PLL_CLK_CONDD
#define PLL_CLK_CONDD   (*(volatile unsigned int*)(CONFIG_BASE + 0x010C))
#endif
#ifndef RG_MSDC2_26M_SEL
#define RG_MSDC2_26M_SEL (1u<<4)
#endif

// GPIO pin numbers (as used in reference) for MSDC2 single-bit
// Adjust if board routing differs.
#define GPIO_MC2_CMD    27
#define GPIO_MC2_CLK    28
#define GPIO_MC2_DAT0   29

// Simple debug macro (USB_Print style assumed available via printf fallback)
#ifndef SDM_DEBUG
#define SDM_DEBUG 1
#endif
#if SDM_DEBUG
#define SDM_LOG(fmt, ...) USB_Print("SDM: " fmt, ##__VA_ARGS__)
#else
#define SDM_LOG(fmt, ...)
#endif

static int g_cardType = SDM_CARD_NONE; // internal card type
static unsigned short g_rca = 0;
static int g_activeController = -1; // 0 or 2
static unsigned g_capacityMB = 0; // computed after CMD9

static void delay_ms_local(unsigned ms){ while(ms--) USC_Pause_us(1000); }

static void msdc_power_on_ctrl(int ctrl_id)
{
#ifdef VMC_CON0
    VMC_CON0 = RG_VMC_EN | VMC_ON_SEL | RG_VMC_VOSEL(VMC_VO33V);
#endif
    delay_ms_local(5);
    if (ctrl_id == 0) PCTL_PowerUp(PD_MSDC); else PCTL_PowerUp(PD_MSDC2);
    delay_ms_local(1);
    delay_ms_local(20);
}

static void msdc_root_clk(void)
{
    unsigned before = PLL_CLK_CONDD;
    PLL_CLK_CONDD = before | RG_MSDC2_26M_SEL;
    unsigned after = PLL_CLK_CONDD;
    SDM_LOG("rootclk %08X->%08X\n", before, after);
}

static void msdc_gpio_mux_ctrl(int ctrl_id)
{
#ifdef GPMODE
    if (ctrl_id == 2) {
        GPIO_Setup(GPIO_MC2_CMD, GPMODE(GPIO27_MODE_MC2CM0));
        GPIO_Setup(GPIO_MC2_CLK, GPMODE(GPIO28_MODE_MC2CK));
        GPIO_Setup(GPIO_MC2_DAT0, GPMODE(GPIO29_MODE_MC2DA0));
    } else if (ctrl_id == 0) {
        GPIO_Setup(31, GPMODE(GPIO31_MODE_MCCK));
        GPIO_Setup(32, GPMODE(GPIO32_MODE_MCCM0));
        GPIO_Setup(33, GPMODE(GPIO33_MODE_MCDA0));
    }
#endif
}

static void msdc_reset_fifo_base(uint32_t base)
{
    volatile uint32_t *cfg = (uint32_t*)(base + 0x0000);
    *cfg |= SDM_MSDC_CFG_RST;
    for (unsigned t=0; t<100000; ++t) if (!(*cfg & SDM_MSDC_CFG_RST)) break;
}

static void msdc_set_clock_khz_base(uint32_t base, unsigned khz)
{
    if (!khz) return;
    // Select MPLL/8 (value 2) as reference, divider field bits[15:8]
    volatile uint32_t *cfgp = (uint32_t*)(base + 0x0000);
    *cfgp &= ~SDM_MSDC_CFG_CLKSRC_Msk;
    *cfgp |= (2u << SDM_MSDC_CFG_CLKSRC_Pos);
    // Base reference after /8 is (approx) 26MHz/8 = 3.25MHz if sourced from 26MHz; adjust logic simple
    // Use same algorithm as reference: compute divisor (cfg) so: op_clock = 26MHz / (4*cfg) when cfg>0 else /2
    unsigned target = khz; // desired kHz
    const unsigned base_khz = 26000; // 26MHz
    unsigned cfg;
    unsigned div = (base_khz + target - 1)/target;
    if (div > 2) {
        cfg = (div >> 2) + ((div & 3)?1:0);
        if (cfg == 0) cfg = 1;
    } else {
        cfg = 0; // special: divide by 2 path
    }
    *cfgp &= ~SDM_MSDC_CFG_SCLKF_Msk;
    *cfgp |= (cfg << SDM_MSDC_CFG_SCLKF_Pos) & SDM_MSDC_CFG_SCLKF_Msk;
    // Enable sample clock output pulse once
    *cfgp |= SDM_MSDC_CFG_SCKON;
    USC_Pause_us(200);
    *cfgp &= ~SDM_MSDC_CFG_SCKON;
    SDM_LOG("clk base=%08lX khz=%u cfg=%u\n", (unsigned long)base, khz, cfg);
}

static int wait_cmd_done_base(uint32_t base, unsigned timeout_us)
{
    volatile uint32_t *sta = (uint32_t*)(base + 0x0004);   // MSDC_STA
    volatile uint32_t *cmdsta = (uint32_t*)(base + 0x0040); // SDC_CMDSTA (clears on read)
    // Phase 1: ensure controller entered busy (optional best-effort)
    unsigned saw_busy = 0;
    for (unsigned i=0; i<100 && timeout_us; ++i) {
        if ( ((*sta) & SDM_MSDC_STA_BUSY) == 0 ) { saw_busy = 1; break; }
        USC_Pause_us(1); --timeout_us;
    }
    // Phase 2: wait until idle (BUSY bit ==1 per datasheet semantics) or timeout
    while (timeout_us--) {
        unsigned v = *sta;
        if (v & SDM_MSDC_STA_BUSY) {
            // Now idle; take single snapshot of CMDSTA
            unsigned c = *cmdsta;
            if (c & SDM_SDC_CMDTO) return -1;
            if (c & SDM_SDC_RSPCRCERR) return -2;
            // Treat absence of CMDRDY as success (it may have self-cleared before read)
            return 0;
        }
        USC_Pause_us(1);
    }
    (void)saw_busy; // silence if unused
    return -3; // host timeout
}

static int send_cmd_base(uint32_t base, unsigned cmd, unsigned arg)
{
    unsigned idx = cmd & 0x3F;
    volatile uint32_t *cmdsta = (uint32_t*)(base + 0x0040); // clear-on-read
    volatile uint32_t *argp   = (uint32_t*)(base + 0x0028);
    volatile uint32_t *cmdp   = (uint32_t*)(base + 0x0024);
    volatile uint32_t *sta    = (uint32_t*)(base + 0x0004); // MSDC_STA
    volatile uint32_t *r0     = (uint32_t*)(base + 0x0030);
    volatile uint32_t *r1     = (uint32_t*)(base + 0x0034);
    volatile uint32_t *r2     = (uint32_t*)(base + 0x0038);
    volatile uint32_t *r3     = (uint32_t*)(base + 0x003C);

    // Clear lingering status from prior command
    (void)*cmdsta;
    unsigned pre_sta = *sta;
    SDM_LOG("CMD%u: ARG=%08lX raw=%04X PRE_STA=%08lX\n", idx, (unsigned long)arg, cmd, (unsigned long)pre_sta);

    *argp = arg;
    *cmdp = cmd; // launch command

    unsigned timeout_us = 20000; // 20ms for non-data commands
    unsigned snapshot = 0;
    while (timeout_us--) {
        unsigned c = *cmdsta; // reading clears
        if (c) { snapshot = c; break; }
        USC_Pause_us(1);
    }
    if (!snapshot) {
        // No bits seen (could have been very quick). Continue; responses should be valid if any.
    } else {
        if (snapshot & SDM_SDC_CMDTO) { SDM_LOG("CMD%u: TIMEOUT snapshot=%08lX\n", idx, (unsigned long)snapshot); return -1; }
        if (snapshot & SDM_SDC_RSPCRCERR) { SDM_LOG("CMD%u: CRCERR snapshot=%08lX\n", idx, (unsigned long)snapshot); return -2; }
    }

    if (idx == 0) {
        // No response expected; nothing to log further.
    } else if (idx == 2 || idx == 9) { // R2 responses (CID/CSD)
        unsigned v0 = *r0, v1 = *r1, v2 = *r2, v3 = *r3;
        SDM_LOG("CMD%u:R2 RESP3=%08lX RESP2=%08lX RESP1=%08lX RESP0=%08lX\n", idx,
            (unsigned long)v3,(unsigned long)v2,(unsigned long)v1,(unsigned long)v0);
        if (idx == 2) {
            unsigned mid = (v3 >> 24) & 0xFF;
            SDM_LOG("CID MID=%02X\n", mid);
        }
    } else {
        unsigned v0 = *r0;
        SDM_LOG("CMD%u:RESP0=%08lX\n", idx, (unsigned long)v0);
        if (idx == 3) { // R6 published RCA in upper 16 bits
            g_rca = (unsigned short)(v0 >> 16);
            SDM_LOG("RCA extracted=%04X\n", g_rca);
        }
    }
    return 0;
}

static int send_acmd41_base(uint32_t base, unsigned arg_base)
{
    // CMD55 first
    if (send_cmd_base(base, SDM_CMD55_APP_CMD, 0)) return -1;
    if (send_cmd_base(base, SDM_ACMD41_SD_OP_COND, arg_base)) return -2;
    return 0;
}

static const char *g_failStage = NULL;

static void msdc_ctrl_reset_base(uint32_t base)
{
    volatile uint32_t *cfg = (uint32_t*)(base + 0x0000);
    *cfg |= SDM_MSDC_CFG_RST | SDM_MSDC_CFG_MSDC; // assert reset while enabling MSDC mode
    // Wait for reset bit to clear
    for (unsigned t=0; t<100000; ++t) {
        if(((*cfg) & SDM_MSDC_CFG_RST)==0) break;
    }
}

static void msdc_dump_regs(uint32_t base, const char *tag)
{
    SDM_LOG("%s DUMP @%08lX: CFG=%08lX STA=%08lX INT=%08lX PS=%08lX IOCON=%08lX IOCON1=%08lX SDC_CFG=%08lX CMDSTA=%08lX DATSTA=%08lX\n",
        tag,
        (unsigned long)base,
        (unsigned long)*(volatile uint32_t*)(base + 0x0000),
        (unsigned long)*(volatile uint32_t*)(base + 0x0004),
        (unsigned long)*(volatile uint32_t*)(base + 0x0008),
        (unsigned long)*(volatile uint32_t*)(base + 0x000C),
        (unsigned long)*(volatile uint32_t*)(base + 0x0014),
        (unsigned long)*(volatile uint32_t*)(base + 0x0018),
        (unsigned long)*(volatile uint32_t*)(base + 0x0020),
        (unsigned long)*(volatile uint32_t*)(base + 0x0040),
        (unsigned long)*(volatile uint32_t*)(base + 0x0044));
}

static boolean try_init_base(uint32_t base, int ctrl_id)
{
    g_cardType = SDM_CARD_NONE;
    msdc_root_clk(); // root clock (shared assumption)
    msdc_power_on_ctrl(ctrl_id);
    msdc_gpio_mux_ctrl(ctrl_id);

    msdc_dump_regs(base, "PRE");

    // Reset controller core
    msdc_ctrl_reset_base(base);

    // Configure basic: enable MSDC, power pin high (VDDPD), enable pin change detect (PINEN), choose clock source 0, set small FIFO threshold
    volatile uint32_t *cfgp = (uint32_t*)(base + 0x0000);
    unsigned cfg = SDM_MSDC_CFG_MSDC | SDM_MSDC_CFG_VDDPD | SDM_MSDC_CFG_PINEN | SDM_MSDC_CFG_RCDEN | SDM_MSDC_CFG_FIFOTHD(1);
    *cfgp = cfg;

    // IO control defaults
    *(volatile uint32_t*)(base + 0x0018) = 0x00022222; // IOCON1 drive strength baseline
    *(volatile uint32_t*)(base + 0x0014) |= (1u<<21);  // SAMPON

    // SD controller specific config: timeout high word and block length + INTEN
    *(volatile uint32_t*)(base + 0x0020) = 0x50018000u | 512u | SDM_SDC_CFG_INTEN; 

    msdc_reset_fifo_base(base);
    // Enable card detect: CDEN + PIEN0, then read back after 32 cycles (we just delay a bit)
    volatile uint32_t *ps = (uint32_t*)(base + 0x000C);
    *ps |= SDM_MSDC_PS_CDEN | SDM_MSDC_PS_PIEN0; // enable detection + input
    delay_ms_local(2);
    unsigned psval = *ps; // reading also clears PINCHG if set
    SDM_LOG("Detect(ctrl=%d): PS=%08lX (PIN0=%u PINCHG=%u)\n", ctrl_id, (unsigned long)psval, (psval & SDM_MSDC_PS_PIN0)?1:0, (psval & SDM_MSDC_PS_PINCHG)?1:0);
    msdc_dump_regs(base, "POSTCFG");

    msdc_set_clock_khz_base(base, 240); // identification ~240 kHz

    if (send_cmd_base(base, SDM_CMD0_GO_IDLE, 0)) { g_failStage = "CMD0"; return false; }
    // Some cards require a second CMD0 after clocks stable
    delay_ms_local(5);
    send_cmd_base(base, SDM_CMD0_GO_IDLE, 0); // ignore result

    // CMD8 (IFCOND)
    delay_ms_local(10); // allow card internal power up before CMD8
    int r = send_cmd_base(base, SDM_CMD8_SEND_IFCOND, CMD8_ARG_PATTERN);
    unsigned ifcond = *(volatile uint32_t*)(base + 0x0030);
    boolean v2 = (r == 0) && ((ifcond & 0xFFF) == CMD8_ARG_PATTERN);
    SDM_LOG("CMD8 r=%d RES0=%08lX expect=000001AA v2=%d (pattern=%03lX)\n", r, (unsigned long)ifcond, v2, (unsigned long)(ifcond & 0xFFF));

    // ACMD41 loop
    unsigned arg = v2 ? ACMD41_ARG_HCS : ACMD41_ARG_SDSC;
    unsigned loops = 200; // 200 * 5ms = 1s max
    while (loops--) {
    if (send_acmd41_base(base, arg)) { g_failStage = "ACMD41 loop"; return false; }
    unsigned ocr = *(volatile uint32_t*)(base + 0x0030);
        SDM_LOG("ACMD41 loop OCR=%08lX busy=%u CCS=%u loops_left=%u\n", (unsigned long)ocr, (ocr>>31)&1, (ocr>>30)&1, loops);
        if (ocr & 0x80000000u) { // busy cleared
            g_cardType = (ocr & 0x40000000u) ? SDM_CARD_SDHC : SDM_CARD_SDSC;
            SDM_LOG("ACMD41 done OCR=%08lX type=%d\n", (unsigned long)ocr, g_cardType);
            break;
        }
        delay_ms_local(5);
    }
    if (g_cardType == SDM_CARD_NONE) { g_failStage = "ACMD41 timeout"; return false; }

    if (send_cmd_base(base, SDM_CMD2_ALL_SEND_CID, 0)) { g_failStage = "CMD2"; return false; }

    if (send_cmd_base(base, SDM_CMD3_SEND_REL_ADDR, 0)) { g_failStage = "CMD3"; return false; }
    SDM_LOG("RCA final=%04X\n", g_rca);

    if (send_cmd_base(base, SDM_CMD9_SEND_CSD, g_rca << 16)) { /* optional */ }
    else {
        // Capture CSD registers (already logged in send_cmd_base). For v2.0 high capacity:
        // CSD_STRUCTURE (bits 127:126) should be 1. C_SIZE spans bits 69:48 (22 bits) across RESP2/RESP1.
        unsigned r0 = *(volatile uint32_t*)(base + 0x0030); // least significant
        unsigned r1 = *(volatile uint32_t*)(base + 0x0034);
        unsigned r2 = *(volatile uint32_t*)(base + 0x0038);
        unsigned r3 = *(volatile uint32_t*)(base + 0x003C); // most significant
        unsigned csd_structure = (r3 >> 30) & 0x3;
        if (csd_structure == 1) {
            // Assemble 128-bit into bit addressing view:
            // Bits 127..96 = r3, 95..64 = r2, 63..32 = r1, 31..0 = r0
            // C_SIZE bits 69..48 -> within r2 (bits 69..64 are r2 bits 5..0) and r1 (bits 63..48 are r1 bits 31..16)
            unsigned part_high = r2 & 0x3F; // bits 5..0 = C_SIZE[21:16]
            unsigned part_low  = (r1 >> 16) & 0xFFFF; // bits 31..16 = C_SIZE[15:0]
            unsigned c_size = (part_high << 16) | part_low; // 22-bit value
            // Capacity (bytes) = (c_size + 1) * 512K (per SD spec for CSD v2.0)
            unsigned long long bytes = ((unsigned long long)(c_size + 1)) * 512ull * 1024ull;
            g_capacityMB = (unsigned)(bytes / (1024ull * 1024ull));
            SDM_LOG("Capacity: C_SIZE=%u -> %u MB\n", c_size, g_capacityMB);
        } else if (csd_structure == 0) {
            // CSD Version 1.0 (SDSC) layout
            // C_SIZE: bits 73..62 (12 bits) spanning r2[9:0] and r1[31:30]
            unsigned csize_high = r2 & 0x3FF;      // r2 bits 9:0 = C_SIZE[11:2]
            unsigned csize_low  = (r1 >> 30) & 0x3; // r1 bits 31:30 = C_SIZE[1:0]
            unsigned c_size = (csize_high << 2) | csize_low; // 12 bits
            // C_SIZE_MULT: bits 49..47 -> r1 bits (49-32)=17..15
            unsigned c_size_mult = (r1 >> 15) & 0x7;
            // READ_BL_LEN: bits 83..80 -> r2 bits (83-64)=19..16
            unsigned read_bl_len = (r2 >> 16) & 0xF;
            unsigned block_len = 1u << read_bl_len;
            unsigned mult = 1u << (c_size_mult + 2); // 2^(C_SIZE_MULT+2)
            unsigned long long blocknr = (unsigned long long)(c_size + 1) * mult; // number of blocks
            unsigned long long bytes = blocknr * block_len;
            g_capacityMB = (unsigned)(bytes / (1024ull * 1024ull));
            SDM_LOG("Capacity(v1): C_SIZE=%u C_SIZE_MULT=%u BL_LEN=%u -> %u MB\n", c_size, c_size_mult, block_len, g_capacityMB);
        } else {
            SDM_LOG("Capacity: Unsupported CSD_STRUCTURE=%u (only v2 handled)\n", csd_structure);
        }
    }

    if (send_cmd_base(base, SDM_CMD7_SELECT_CARD, g_rca << 16)) { g_failStage = "CMD7"; return false; }

    // Switch to higher clock ~13 MHz
    msdc_set_clock_khz_base(base, 13000);

    g_activeController = ctrl_id;
    return true;
}

boolean SDM_Init(void)
{
    g_activeController = -1;
    // Probe MSDC0 first
    if (try_init_base(0xA0130000u, 0)) return true;
    // If failed, clear failStage (retain last) and try MSDC2
    if (try_init_base(0xA0270000u, 2)) return true;
    return false;
}

boolean SDM_ReadBlock0(uint8_t *buf)
{
    return SDM_ReadBlock(0, buf);
}

boolean SDM_ReadBlock(uint32_t lba, uint8_t *buf)
{
    if (!buf) return false;
    if (g_cardType == SDM_CARD_NONE) return false;
    uint32_t base = (g_activeController==0)?0xA0130000u:0xA0270000u;
    *(volatile uint32_t*)(base + 0x0020) = (*(volatile uint32_t*)(base + 0x0020) & 0xFFFF0000u) | 512u;
    unsigned arg = (g_cardType == SDM_CARD_SDHC) ? lba : (lba * 512u);
    if (send_cmd_base(base, SDM_CMD17_READ_SINGLE, arg)) return false;
    uint32_t *p = (uint32_t*)buf;
    unsigned words = 512/4;
    unsigned timeout = 2000000;
    volatile uint32_t *sta = (uint32_t*)(base + 0x0004);
    volatile uint32_t *dat = (uint32_t*)(base + 0x0010);
    while (words && timeout--) {
        if (*sta & SDM_MSDC_STA_DRQ) { *p++ = *dat; --words; }
    }
    if (words) {
        SDM_LOG("READ timeout LBA=%lu remain=%u STA=%08lX DATSTA=%08lX\n", (unsigned long)lba, words,
            (unsigned long)*sta, (unsigned long)*(volatile uint32_t*)(base + 0x0044));
        msdc_reset_fifo_base(base);
        *sta |= SDM_MSDC_STA_FIFOCLR;
        return false;
    }
    *sta |= SDM_MSDC_STA_FIFOCLR;
    msdc_reset_fifo_base(base);
    return true;
}

int SDM_GetCardType(void)
{
    return g_cardType;
}

const char *SDM_GetLastFailStage(void)
{
    return g_failStage;
}
int SDM_GetActiveController(void)
{
    return g_activeController;
}

unsigned SDM_GetCapacityMB(void)
{
    return g_capacityMB;
}

unsigned SDM_CardDetectRaw(void)
{
    if (g_activeController < 0) return 0;
    uint32_t base = (g_activeController==0)?0xA0130000u:0xA0270000u; // same bases used earlier
    return *(volatile uint32_t*)(base + 0x000C);
}
