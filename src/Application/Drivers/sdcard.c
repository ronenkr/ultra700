/*
 * Simple SD card wrapper built on top of low-level msdc driver.
 */
#include "systemconfig.h"
#include "sdcard.h"
#include "usb_print.h"   // USB_Print
#include <string.h>

// Attempt full init and fill optional info struct
bool SDCARD_Initialize(sdcard_info_t *outInfo)
{
	int res = SD_Init();
	if (res) {
		USB_Print("SDCARD: init failed (%d)\n", res);
		if (outInfo) memset(outInfo, 0, sizeof(*outInfo));
		return false;
	}

	// Access global context from msdc.c
	extern sd_ctx_t sd_ctx; // declared in msdc.c
	if (outInfo) {
		outInfo->present   = true;
		outInfo->capacity  = sd_ctx.csd.capacity; // already bytes according to code path
		outInfo->block_size = SECTOR_SIZE;
		outInfo->card_type = (uint8_t)sd_ctx.card_type;
	}

	USB_Print("SDCARD: init OK, type=%u, capacity=%lu KB, block=%u bytes\n",
			  (unsigned)sd_ctx.card_type,
			  (unsigned long)(sd_ctx.csd.capacity / 1024u),
			  (unsigned)SECTOR_SIZE);
	return true;
}

bool SDCARD_ReadSectors(uint32_t lba, void *buffer, uint32_t count)
{
	if (!buffer || count == 0) return false;
	int res = SD_ReadBlock(lba, buffer, count);
	if (res) {
		USB_Print("SDCARD: read fail lba=%lu cnt=%lu (err=%d)\n",
				  (unsigned long)lba, (unsigned long)count, res);
		return false;
	}
	return true;
}
