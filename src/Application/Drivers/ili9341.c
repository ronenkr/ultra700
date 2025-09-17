// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/*
* This file is part of the DZ09 project.
*
* Copyright (C) 2019 AJScorp
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
#include "appconfig.h"
#include "ili9341.h"
#include "lcdif.h"

boolean ILI9341_Initialize(void)
{
    LCDIF_WriteCommand(ILI9341_PCONTROL1);
    LCDIF_WriteData(VRH(0x23));

    LCDIF_WriteCommand(ILI9341_PCONTROL2);
    LCDIF_WriteData(BT(0x10));

    LCDIF_WriteCommand(ILI9341_VCOMCONTROL1);
    LCDIF_WriteData(VMH(0x3E));
    LCDIF_WriteData(VMH(0x28));

    LCDIF_WriteCommand(ILI9341_MADCTL);
    // Old: LCDIF_WriteData(0x28 | MC_BGR);  // MV (bit5) set -> rotation causing stride mismatch
    // New: no MV, keep BGR (adjust MX/MY if you need flipping)
    LCDIF_WriteData(MC_MV|MC_RGB|MC_MY|MC_MX);            // 0x08 : normal (no rotation), BGR color order
    // If you actually want a flip, examples:
    //   MC_MY | MC_BGR        (vertical flip)
    //   MC_MX | MC_BGR        (horizontal flip)
    //   MC_MX | MC_MY | MC_BGR (180°)
    // Avoid MC_MV unless you also swap X/Y usage and pitch logic.

    LCDIF_WriteCommand(ILI9341_PIXFMT);
    LCDIF_WriteData(0x55); //changed to 16Bit , DPI(DPI18bit) | DBI(DBI18bit)

    LCDIF_WriteCommand(ILI9341_FRAMERATE);
    LCDIF_WriteData(DIVA(0x00));
    LCDIF_WriteData(RTNA(0x18));

    LCDIF_WriteCommand(ILI9341_DFUNCCONTROL);
    LCDIF_WriteData(PTG(0x02) | PT(0x00));
    LCDIF_WriteData(REV | ISC(0x02));
    LCDIF_WriteData(NL(0x1D));

    LCDIF_WriteCommand(ILI9341_SLPOUT);
    LCDIF_WriteCommand(ILI9341_DISPON);

    // Enable display color inversion per user request
    LCDIF_WriteCommand(ILI9341_INVON);

    return true;                                                                                    // I can't read the display ID, so here just returns true.
}

void ILI9341_SetInversion(boolean invert)
{
    LCDIF_WriteCommand(invert ? ILI9341_INVON : ILI9341_INVOFF);
}


void ILI9341_DrawFilledRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color)
{
    if (LCDScreen.VLayer[0].Initialized && LCDScreen.VLayer[0].FrameBuffer)
    {
        if (x >= LCD_XRESOLUTION || y >= LCD_YRESOLUTION) return;
        if (x + width  > LCD_XRESOLUTION) width  = LCD_XRESOLUTION - x;
        if (y + height > LCD_YRESOLUTION) height = LCD_YRESOLUTION - y;
        uint16_t *fb = (uint16_t*)LCDScreen.VLayer[0].FrameBuffer;
        for (uint16_t row = 0; row < height; row++)
        {
            uint16_t *dst = fb + (y + row) * LCD_XRESOLUTION + x;
            for (uint16_t col = 0; col < width; col++) dst[col] = color;
        }
        TRECT rct = { (int16_t)x, (int16_t)y, (int16_t)(x + width - 1), (int16_t)(y + height - 1) };
        LCDIF_UpdateRectangle(rct);
    }
    else
    {
        // Fallback legacy direct command path if layer not initialized
        uint32_t totalPixels = width * height;
        LCDIF_WriteCommand(0x2A);
        LCDIF_WriteData(x >> 8);
        LCDIF_WriteData(x & 0xFF);
        LCDIF_WriteData((x + width - 1) >> 8);
        LCDIF_WriteData((x + width - 1) & 0xFF);
        LCDIF_WriteCommand(0x2B);
        LCDIF_WriteData(y >> 8);
        LCDIF_WriteData(y & 0xFF);
        LCDIF_WriteData((y + height - 1) >> 8);
        LCDIF_WriteData((y + height - 1) & 0xFF);
        LCDIF_WriteCommand(0x2C);
        for (uint32_t i = 0; i < totalPixels; i++) {
            LCDIF_WriteData(color >> 8);
            LCDIF_WriteData(color & 0xFF);
        }
    }
}

void ILI9341_SleepLCD(void)
{
    LCDIF_WriteCommand(ILI9341_DISPOFF);
    LCDIF_WriteCommand(ILI9341_SLPIN);
}

void ILI9341_ResumeLCD(void)
{
    LCDIF_WriteCommand(ILI9341_SLPOUT);
    LCDIF_WriteCommand(ILI9341_DISPON);
}

uint32_t *ILI9341_SetOutputWindow(pRECT Rct, uint32_t *Count, uint32_t DataAttr, uint32_t CmdAttr)
{
    uint32_t *Data = NULL;

    if ((Rct != NULL) && (Count != NULL))
    {
        uint32_t i = 0;

        Data = malloc(ILI9341_SETWINCMDSIZE * sizeof(uint32_t));
        if (Data != NULL)
        {
            Data[i++] = LCDIF_COMM(ILI9341_CASET) | CmdAttr;
            Data[i++] = LCDIF_COMM(CASSH(Rct->l)) | DataAttr;
            Data[i++] = LCDIF_COMM(CASSL(Rct->l)) | DataAttr;
            Data[i++] = LCDIF_COMM(CASEH(Rct->r)) | DataAttr;
            Data[i++] = LCDIF_COMM(CASEL(Rct->r)) | DataAttr;

            Data[i++] = LCDIF_COMM(ILI9341_RASET) | CmdAttr;
            Data[i++] = LCDIF_COMM(RASSH(Rct->t + ILI9341_ROWSHIFT)) | DataAttr;
            Data[i++] = LCDIF_COMM(RASSL(Rct->t + ILI9341_ROWSHIFT)) | DataAttr;
            Data[i++] = LCDIF_COMM(RASEH(Rct->b + ILI9341_ROWSHIFT)) | DataAttr;
            Data[i++] = LCDIF_COMM(RASEL(Rct->b + ILI9341_ROWSHIFT)) | DataAttr;

            Data[i++] = LCDIF_COMM(ILI9341_RAMWR) | CmdAttr;

            *Count = i;
        }
    }
    return Data;

}


void ILI9341_DirectPixelWrite(uint16_t x, uint16_t y, uint16_t color)
{
    // Direct pixel write to framebuffer (if exists)
    if (LCDScreen.VLayer[0].Initialized && LCDScreen.VLayer[0].FrameBuffer)
    {
        uint16_t *fb = (uint16_t*)LCDScreen.VLayer[0].FrameBuffer;
        if (x < LCD_XRESOLUTION && y < LCD_YRESOLUTION)
        {
            fb[y * LCD_XRESOLUTION + x] = color;

            // Update just this pixel area
            TRECT pixel_rect = {x, y, x, y};
            LCDIF_UpdateRectangle(pixel_rect);
        }
    }
    else
    {
        DebugPrint("ILI9341: No framebuffer available for direct write\n");
    }
}

void ILI9341_FlushFrameBuffer(void)
{
    // Force the LCD interface to update the physical display
    if (LCDScreen.VLayer[0].Initialized && LCDScreen.VLayer[0].FrameBuffer)
    {
        TRECT full_screen;
        full_screen.l = 0;
        full_screen.t = 0;
        full_screen.r = LCD_XRESOLUTION - 1;
        full_screen.b = LCD_YRESOLUTION - 1;

        // Set the output window to full screen
        uint32_t cmd_count;
        uint32_t *cmd_data = ILI9341_SetOutputWindow(&full_screen, &cmd_count,
                                                     LCDIF_DATA,
                                                     LCDIF_CMD);

        if (cmd_data != NULL)
        {
            // Send the command sequence to set output window
            LCDIF_AddCommandToQueue(cmd_data, cmd_count, &full_screen);
            free(cmd_data);

            // Now force the framebuffer data to be sent
            LCDIF_UpdateRectangle(full_screen);
        }

        DebugPrint("ILI9341: Flushed framebuffer to display\n");
    }
    else
    {
        DebugPrint("ILI9341: No framebuffer to flush\n");
    }
}

void ILI9341_UpdateRect(TRECT rect)
{
    // Update a specific rectangle on the display
    uint32_t cmd_count;
    uint32_t *cmd_data = ILI9341_SetOutputWindow(&rect, &cmd_count,
                                                 LCDIF_DATA,
                                                 LCDIF_CMD);

    if (cmd_data != NULL)
    {
        LCDIF_AddCommandToQueue(cmd_data, cmd_count, &rect);
        free(cmd_data);
        LCDIF_UpdateRectangle(rect);
        //DebugPrint("ILI9341: Updated rect (%d,%d,%d,%d)\n", rect.l, rect.t, rect.r, rect.b);
    }
}

