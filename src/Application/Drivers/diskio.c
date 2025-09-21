/*-----------------------------------------------------------------------*/
/* Disk I/O glue for FatFs using sd_minimal on MT6261                    */
/*-----------------------------------------------------------------------*/

#include "ff.h"
#include "diskio.h"
#include "sd_minimal.h"

#define DEV_SD   0  /* Physical drive 0 maps to the SD card */

static DSTATUS s_stat = STA_NOINIT;

static int sd_card_present(void)
{
	/* If card detect is available, use it; otherwise assume present */
	/* SDM_CardDetectRaw returns non-zero when a card is present */
	return SDM_CardDetectRaw() ? 1 : 0;
}

DSTATUS disk_status(BYTE pdrv)
{
	if (pdrv != DEV_SD) return STA_NOINIT;

	if (!sd_card_present()) {
		s_stat = STA_NOINIT | STA_NODISK;
	}
	return s_stat;
}

DSTATUS disk_initialize(BYTE pdrv)
{
	if (pdrv != DEV_SD) return STA_NOINIT;

	if (!sd_card_present()) {
		s_stat = STA_NOINIT | STA_NODISK;
		return s_stat;
	}

	if (SDM_Init()) {
		s_stat = 0; /* Initialized */
	} else {
		s_stat = STA_NOINIT;
	}
	return s_stat;
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count)
{
	if (pdrv != DEV_SD || !buff || count == 0) return RES_PARERR;
	if (s_stat & STA_NOINIT) return RES_NOTRDY;

	/* sd_minimal provides single-block reads; loop for multi-block */
	for (UINT i = 0; i < count; ++i) {
		if (!SDM_ReadBlock((uint32_t)(sector + i), buff + (i * 512u))) {
			return RES_ERROR;
		}
	}
	return RES_OK;
}

#if FF_FS_READONLY == 0
DRESULT disk_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count)
{
	(void)buff; (void)sector; (void)count;
	if (pdrv != DEV_SD) return RES_PARERR;
	/* Writes are not supported by sd_minimal glue */
	return RES_WRPRT;
}
#endif

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
	if (pdrv != DEV_SD) return RES_PARERR;

	switch (cmd) {
	case CTRL_SYNC:
		return RES_OK;
	case GET_SECTOR_SIZE:
		if (!buff) return RES_PARERR;
		*(WORD*)buff = 512; /* Fixed 512B sectors */
		return RES_OK;
	case GET_BLOCK_SIZE:
		if (!buff) return RES_PARERR;
		*(DWORD*)buff = 1; /* Erase block size in units of sector */
		return RES_OK;
	case GET_SECTOR_COUNT:
		if (!buff) return RES_PARERR;
		{
			unsigned mb = SDM_GetCapacityMB();
			/* Convert MB to sector count (MB * 1024 * 1024 / 512) */
			DWORD sc = (DWORD)mb * 2048u;
			*(LBA_t*)buff = sc;
		}
		return RES_OK;
	default:
		return RES_PARERR;
	}
}

