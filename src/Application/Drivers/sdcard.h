/*
* This file is part of the DZ09 project.
*
* Copyright (C) 2022 AJScorp
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
#ifndef _SDCARD_H_
#define _SDCARD_H_

// Basic SD / FAT structures / low level driver
#include "msdc.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Lightweight public info (subset of msdc internal state)
typedef struct {
	bool     present;      // card initialized & selected
	uint64_t capacity;     // bytes (0 if unknown)
	uint32_t block_size;   // usually 512
	uint8_t  card_type;    // hal_sd_card_type_t
} sdcard_info_t;

// Initialize controller & card. Returns true on success.
bool SDCARD_Initialize(sdcard_info_t *outInfo);

// Read 'count' 512-byte sectors starting at logical sector 'lba' into buffer.
// Returns true on success.
bool SDCARD_ReadSectors(uint32_t lba, void *buffer, uint32_t count);

#ifdef __cplusplus
}
#endif


#endif /* _SDCARD_H_ */
