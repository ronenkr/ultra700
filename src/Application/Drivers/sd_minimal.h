#ifndef SD_MINIMAL_H
#define SD_MINIMAL_H
#include <stdint.h>
#include "systypes.h" // for boolean typedef

// Focus: very small subset needed for init + single block read on MT6261 MSDC2
// Clean-room style minimal definitions (no direct large GPL text copying)

#define MSDC2_BASE 0xA0270000u

// Register offsets (word access)
#define SDM_MSDC_CFG        (*(volatile uint32_t*)(MSDC2_BASE + 0x0000))
#define SDM_MSDC_STA        (*(volatile uint32_t*)(MSDC2_BASE + 0x0004))
#define SDM_MSDC_INT        (*(volatile uint32_t*)(MSDC2_BASE + 0x0008))
#define SDM_MSDC_PS         (*(volatile uint32_t*)(MSDC2_BASE + 0x000C))
#define SDM_MSDC_DAT        (*(volatile uint32_t*)(MSDC2_BASE + 0x0010))
#define SDM_MSDC_IOCON      (*(volatile uint32_t*)(MSDC2_BASE + 0x0014))
#define SDM_MSDC_IOCON1     (*(volatile uint32_t*)(MSDC2_BASE + 0x0018))
#define SDM_SDC_CFG         (*(volatile uint32_t*)(MSDC2_BASE + 0x0020))
#define SDM_SDC_CMD         (*(volatile uint32_t*)(MSDC2_BASE + 0x0024))
#define SDM_SDC_ARG         (*(volatile uint32_t*)(MSDC2_BASE + 0x0028))
#define SDM_SDC_STA         (*(volatile uint32_t*)(MSDC2_BASE + 0x002C))
#define SDM_SDC_RES0        (*(volatile uint32_t*)(MSDC2_BASE + 0x0030))
#define SDM_SDC_RES1        (*(volatile uint32_t*)(MSDC2_BASE + 0x0034))
#define SDM_SDC_RES2        (*(volatile uint32_t*)(MSDC2_BASE + 0x0038))
#define SDM_SDC_RES3        (*(volatile uint32_t*)(MSDC2_BASE + 0x003C))
#define SDM_SDC_CMDSTA      (*(volatile uint32_t*)(MSDC2_BASE + 0x0040))
#define SDM_SDC_DATSTA      (*(volatile uint32_t*)(MSDC2_BASE + 0x0044))

// SDC_CFG bits (partial)
#define SDM_SDC_CFG_INTEN    (1u<<0)

// MSDC_CFG bits
#define SDM_MSDC_CFG_MSDC       (1u<<0)
#define SDM_MSDC_CFG_RST        (1u<<1)
#define SDM_MSDC_CFG_CLKSRC_Pos 3
#define SDM_MSDC_CFG_CLKSRC_Msk (3u<<SDM_MSDC_CFG_CLKSRC_Pos)
#define SDM_MSDC_CFG_SCLKF_Pos  8
#define SDM_MSDC_CFG_SCLKF_Msk  (0xFFu<<SDM_MSDC_CFG_SCLKF_Pos)
#define SDM_MSDC_CFG_SCKON      (1u<<7)
#define SDM_MSDC_CFG_PINEN      (1u<<18)

// SDC_CMDSTA bits (match msdc.h SDC_CMDRDY=bit0, CMDTO=bit1, RSPCRCERR=bit2)
#define SDM_SDC_CMDRDY          (1u<<0)
#define SDM_SDC_CMDTO           (1u<<1)
#define SDM_SDC_RSPCRCERR       (1u<<2)

// SDC_STA busy bits
#define SDM_SDC_STA_SDCBUSY     (1u<<0)

// FIFO status from MSDC_STA
#define SDM_MSDC_STA_DRQ        (1u<<2)
#define SDM_MSDC_STA_BF         (1u<<0)
#define SDM_MSDC_STA_BE         (1u<<1)
#define SDM_MSDC_STA_BUSY       (1u<<15)
// MSDC_CFG bits (datasheet MT6261D)
#define SDM_MSDC_CFG_FIFOTHD(x)  (((x)&0xF)<<24)
#define SDM_MSDC_CFG_VDDPD       (1u<<21)
#define SDM_MSDC_CFG_RCDEN       (1u<<20)
#define SDM_MSDC_CFG_DIRQEN      (1u<<19)
#define SDM_MSDC_CFG_PINEN       (1u<<18)
#define SDM_MSDC_CFG_DMAEN       (1u<<17)
#define SDM_MSDC_CFG_INTEN       (1u<<16)
#define SDM_MSDC_CFG_SCLKF(x)    (((x)&0xFF)<<8)
#define SDM_MSDC_CFG_SCLKON      (1u<<7)
#define SDM_MSDC_CFG_CRED        (1u<<6)
#define SDM_MSDC_CFG_STDBY       (1u<<5)
#define SDM_MSDC_CFG_CLKSRC(x)   (((x)&0x3)<<3)
#define SDM_MSDC_CFG_NOCRC       (1u<<2)
#define SDM_MSDC_CFG_RST         (1u<<1)
#define SDM_MSDC_CFG_MSDC        (1u<<0)

// MSDC_PS bits (card detect)
#define SDM_MSDC_PS_PINCHG       (1u<<4)
#define SDM_MSDC_PS_PIN0         (1u<<3)
#define SDM_MSDC_PS_POEN0        (1u<<2)
#define SDM_MSDC_PS_PIEN0        (1u<<1)
#define SDM_MSDC_PS_CDEN         (1u<<0)

#define SDM_MSDC_STA_FIFOCLR    (1u<<14)

// Use existing full driver encodings to avoid mismatch
#include "sdcmd.h"  // provides MSDC_CMD* and MSDC_ACMD41 etc.
#define SDM_CMD0_GO_IDLE        MSDC_CMD0
#define SDM_CMD8_SEND_IFCOND    MSDC_CMD8
#define SDM_CMD55_APP_CMD       MSDC_CMD55
#define SDM_ACMD41_SD_OP_COND   MSDC_ACMD41
#define SDM_CMD2_ALL_SEND_CID   MSDC_CMD2
#define SDM_CMD3_SEND_REL_ADDR  MSDC_CMD3
#define SDM_CMD7_SELECT_CARD    MSDC_CMD7
#define SDM_CMD9_SEND_CSD       MSDC_CMD9
#define SDM_CMD17_READ_SINGLE   MSDC_CMD17
#define SDM_CMD12_STOP_TRAN     MSDC_CMD12

// Arguments
#define CMD8_ARG_PATTERN    0x000001AAu
#define ACMD41_ARG_HCS      0x40FF8000u  // High capacity + voltage range
#define ACMD41_ARG_SDSC     0x00FF8000u

// Card type flags
#define SDM_CARD_NONE   0
#define SDM_CARD_SDSC   1
#define SDM_CARD_SDHC   2

#ifdef __cplusplus
extern "C" {
#endif

boolean SDM_Init(void);                 // probe MSDC0 then MSDC2; initialize first responding card
boolean SDM_ReadBlock0(uint8_t *buf);   // read LBA0 (512B)
boolean SDM_ReadBlock(uint32_t lba, uint8_t *buf); // generic single block read
int  SDM_GetCardType(void);          // return SDM_CARD_* value
unsigned SDM_CardDetectRaw(void);  // Add prototype for SDM_CardDetectRaw
const char *SDM_GetLastFailStage(void); // NULL if last init succeeded
int SDM_GetActiveController(void);      // 0,2 or -1 if none
unsigned SDM_GetCapacityMB(void);       // 0 if unknown/not init

#ifdef __cplusplus
}
#endif

#endif // SD_MINIMAL_H
