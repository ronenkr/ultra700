// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/*
* This file is part of the DZ09 project.
*
* Copyright (C) 2021 - 2019 AJScorp
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
#include "mtktypes.h"
#include "sha-1.h"
#include "utils.h"
#include "dz09_boot.h"

extern uint32_t __ROMImageBase, __ROMImageLimit;

void *BL_CheckFileByDescriptor(BL_Descr File)
{
    void *ExtBLEntryPoint = NULL;

    File.m_bl_begin_dev_addr += (uint32_t)&__ROMImageBase;
    File.m_bl_boundary_dev_addr += (uint32_t)&__ROMImageBase;

    if ((File.m_bl_begin_dev_addr > (uint32_t)&__ROMImageBase) &&
            (File.m_bl_boundary_dev_addr < (uint32_t)&__ROMImageLimit))
    {
        pFILE_INFO_v1 FileInfo = (pFILE_INFO_v1)File.m_bl_begin_dev_addr;

        DebugPrint(" Ext bootloader base address: 0x%08X, boundary: 0x%08X\r\n",
                   File.m_bl_begin_dev_addr, File.m_bl_boundary_dev_addr);

        /* Checking file */
        if ((GFH_GETMAGIC(FileInfo->hdr.magic) == GFH_HDR_MAGIC) &&
                (FileInfo->hdr.type == GFH_FILE_INFO) &&
                (FileInfo->hdr.size == sizeof(FILE_INFO_v1)) &&
                (FileInfo->file_type == ARM_EXT_BL) &&
                (FileInfo->flash_dev == F_SF) &&
                (FileInfo->load_addr >= (uint32_t)&__ROMImageBase) &&
                (FileInfo->load_addr + FileInfo->file_len < (uint32_t)&__ROMImageLimit))
        {
            uint32_t SizeToCheck = FileInfo->file_len - FileInfo->sig_len;
            pSHA1    CheckedHash;

            DebugPrint(" File size: 0x%08X, load address: 0x%08X\r\n", SizeToCheck, FileInfo->load_addr);
            DebugPrint(" Signature type: %u, size: %u, MTKEND: %08X\r\n", FileInfo->sig_type,
                       FileInfo->sig_len, *(uint32_t *)(FileInfo->load_addr + SizeToCheck - 4));

            if (FileInfo->sig_type == SIG_NONE)
                ExtBLEntryPoint = (void *)(FileInfo->load_addr + FileInfo->jump_offset);
            else if ((FileInfo->sig_type == SIG_PHASH) &&
                     (FileInfo->sig_len == sizeof(MTK_PHASH)) &&
                     (*(uint32_t *)(FileInfo->load_addr + SizeToCheck - 4) == MTK_FEND))
            {
                DebugPrint(" Processing SHA-1 - ");
                CheckedHash = SHA1_ProcessData((uint8_t *)FileInfo->load_addr, SizeToCheck);
                if (CheckedHash != NULL)
                {
                    pMTK_PHASH FileHash = (pMTK_PHASH)(FileInfo->load_addr + SizeToCheck);

                    if ((swab32(CheckedHash->H0) == FileHash->H0) &&
                            (swab32(CheckedHash->H1) == FileHash->H1) &&
                            (swab32(CheckedHash->H2) == FileHash->H2) &&
                            (swab32(CheckedHash->H3) == FileHash->H3) &&
                            (swab32(CheckedHash->H4) == FileHash->H4))
                    {
                        ExtBLEntryPoint = (void *)(FileInfo->load_addr + FileInfo->jump_offset);
                        DebugPrint("complete\r\n");
                        DebugPrint("Entry point %p, jump to BL...\r\n\r\n", ExtBLEntryPoint);
                    }
                    else DebugPrint("failed\r\n");
                }
            }
        }
    }
    return ExtBLEntryPoint;
}

int main(void)
{
    pSF_HEADER_v1 sf_header = (pSF_HEADER_v1)&__ROMImageLimit;

    /* Setup system watchdog */
    RGU_SetWDTInterval(WDTINTERVAL, true);                                                          // Setup system watchdog

    DBG_Initialize();                                                                               // Setup debug interface
    USC_StartCounter();
    PLL_Initialize();

    DebugPrint("\r\n--1st bootloader runs--\r\n");
    DebugPrint("System watchdog was set for %d seconds\r\n", WDTINTERVAL);
    EMI_MemoryRemap(MR_FB1RB0);                                                                     /*  Remap Flash to Bank1, RAM to Bank0.
                                                                                                        Now ROM starts from 0x10000000 */
    DebugPrint("CPU measured frequency: %uMHz\r\n", GetCPUFrequency());

    /* Check SF header for validity */
    if (!strcmp(sf_header->m_identifier, SF_HEADER_ID) &&
            (sf_header->m_ver == SF_HEADER_VER))
    {
        pBR_Layout_v1 BR_Layout;

        DebugPrint("SF header ID: \"%s\"\r\n", sf_header->m_identifier);

        /* Check boot regions layout for validity */
        BR_Layout = (pBR_Layout_v1)(sf_header->m_dev_rw_unit + (uint32_t)&__ROMImageLimit);
        if (!strcmp(BR_Layout->m_identifier, BRLYT_ID) &&
                (BR_Layout->m_ver == BRLYT_VER))
        {
            uint32_t i;
            void    (*Run2ndBoot)(void);

            DebugPrint("Boot regions layout ID: \"%s\", ver: %d\r\n", BR_Layout->m_identifier, BR_Layout->m_ver);

            /* Try to find descriptor of 2nd bootloader */
            for(i = 0; i < MAX_BL_NUM; i++)
            {
                /* Restart watchdog */
                RGU_RestartWDT();

                DebugPrint("Descriptor %d, device %d, type %d\r\n",
                           i, BR_Layout->m_bl_desc[i].m_bl_dev, BR_Layout->m_bl_desc[i].m_bl_type);
                if ((BR_Layout->m_bl_desc[i].m_bl_exist_magic == BL_EXIST_MAGIC) &&
                        (BR_Layout->m_bl_desc[i].m_bl_dev == F_SF) &&
                        (BR_Layout->m_bl_desc[i].m_bl_type == ARM_EXT_BL) &&
                        ((Run2ndBoot = BL_CheckFileByDescriptor(BR_Layout->m_bl_desc[i])) != NULL))
                {
                    Run2ndBoot();
                }
            }
        }
    }
    return 0;
}
