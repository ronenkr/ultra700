// Hardware I2C scanner (MT6261) – avoids keypad GPIO overlap by using dedicated SCL/SDA pins (GPIO43/44)
#include "systemconfig.h"
#include "i2c_scanner.h"
#include "usb_print.h"
#include "mt6261.h"

// Some register/macro definitions may be missing from current mt6261.h; guard to allow local use
#ifndef I2C_base
#define I2C_base 0xA0120000UL
#endif

#ifndef PD_I2C
#define PD_I2C (0x16)
#endif

// Registers (subset)
#define I2C_DATA_PORT_REG       (*(volatile uint16_t *)(I2C_base + 0x0000))
#define I2C_SLAVE_REG           (*(volatile uint16_t *)(I2C_base + 0x0004))
#define I2C_INT_STAT_REG        (*(volatile uint16_t *)(I2C_base + 0x000C))
#define I2C_CONTROL_REG         (*(volatile uint16_t *)(I2C_base + 0x0010))
#define   I2C_CONTROL_ACKERR_DET_EN   (1 << 5)
#define I2C_TRANSFER_LEN_REG    (*(volatile uint16_t *)(I2C_base + 0x0014))
#define I2C_TRANSAC_LEN_REG     (*(volatile uint16_t *)(I2C_base + 0x0018))
#define I2C_TIMING_REG          (*(volatile uint16_t *)(I2C_base + 0x0020))
#define   I2C_TIMING_STEP_SHIFT 0
#define   I2C_TIMING_SAMPLE_SHIFT 8
#define I2C_START_REG           (*(volatile uint16_t *)(I2C_base + 0x0024))
#define I2C_FIFO_ADDR_CLR_REG   (*(volatile uint16_t *)(I2C_base + 0x0038))
#define I2C_IO_CONFIG_REG       (*(volatile uint16_t *)(I2C_base + 0x0040))
#define I2C_SOFTRESET_REG       (*(volatile uint16_t *)(I2C_base + 0x0050))

// Bit masks for INT status
#define I2C_INT_STAT_TRANSAC_COMP   (1 << 0)
#define I2C_INT_STAT_ACKERR         (1 << 1)

static boolean hw_i2c_inited = false;

static void HW_I2C_Init(void)
{
    if (hw_i2c_inited) return;

    // Power up I2C peripheral
    PCTL_PowerUp(PD_I2C);

    // Reset controller
    I2C_SOFTRESET_REG = 1;

    // Configure GPIO43 (SCL), GPIO44 (SDA) alternate functions with pull-ups
    GPIO_Setup(GPIO43, GPMODE(GPIO43_MODE_SCL) | GPPULLEN | GPPUP);
    GPIO_Setup(GPIO44, GPMODE(GPIO44_MODE_SDA) | GPPULLEN | GPPUP);

    // Timing setup for 100 kHz:
    // Half period requirement: 1/(2*100kHz) = 5us
    // Formula (from MT6261 spec): half_period = ((step_div+1)*(sample_div+1)) / 13MHz
    // Solve (step_div+1)*(sample_div+1) = 5us * 13MHz = 65
    // Choose factors: 65 = 13 * 5  => step_div = 12, sample_div = 4
    // Verify: ((12+1)*(4+1))/13e6 = 65 / 13e6 = 5.000us (exact) => full period 10us => 100 kHz
    uint16_t sample = 4; // sample_div
    uint16_t step = 12;  // step_div
    I2C_TIMING_REG = (uint16_t)((step & 0x3F) << I2C_TIMING_STEP_SHIFT) | (uint16_t)((sample & 0x07) << I2C_TIMING_SAMPLE_SHIFT);

    hw_i2c_inited = true;
    USB_Print("HW I2C: Initialized (GPIO43=SCL GPIO44=SDA) @100kHz\n");
}

void I2C_Scanner_Init(void)
{
    HW_I2C_Init();
}

static boolean HW_I2C_Ping(uint8_t addr)
{
    // Clear FIFO & status
    I2C_INT_STAT_REG = 0xFFFF; // clear all
    I2C_FIFO_ADDR_CLR_REG = 1;

    // Single transaction, write phase only (no data bytes) to check ACK of address
    I2C_TRANSAC_LEN_REG = 1;    // one transaction
    I2C_SLAVE_REG = (uint16_t)(addr << 1); // 7-bit addr + write (bit0=0)
    I2C_TRANSFER_LEN_REG = 0;   // 0 data bytes

    I2C_CONTROL_REG = I2C_CONTROL_ACKERR_DET_EN; // enable ACK error detect
    I2C_START_REG = 1;

    // Wait for completion or ACK error (simple polling)
    uint32_t guard = 50000; // rough timeout loop
    while (guard--) {
        uint16_t st = I2C_INT_STAT_REG;
        if (st & (I2C_INT_STAT_TRANSAC_COMP | I2C_INT_STAT_ACKERR)) {
            if (st & I2C_INT_STAT_ACKERR) return false; // NACK
            return true; // got completion without ACKERR
        }
    }
    return false; // timeout treated as no device
}

void I2C_Scanner_Run(void)
{
    if (!hw_i2c_inited) HW_I2C_Init();

    USB_Print("HW I2C Scan start (0x03..0x77)\n");
    uint8_t found = 0;
    for (uint8_t addr = 0x03; addr <= 0x77; addr++) {
        if (HW_I2C_Ping(addr)) {
            found++;
            USB_Print("  Found device at 0x%02X\n", addr);
        }
    }
    if (!found)
        USB_Print("No I2C devices responded.\n");
    else
        USB_Print("I2C Scan complete: %u device(s) found.\n", found);
}
