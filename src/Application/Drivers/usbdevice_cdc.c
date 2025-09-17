// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/*
* This file is part of the DZ09 project.
*
* Copyright (C) 2022 - 2020 AJScorp
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
#include "usb9.h"
#include "usbdevice_cdc.h"

/* CDC interface configuration */
#define USB_CDC_CONTROL_EP          USB_EP3IN
#define USB_CDC_DATAIN_EP           USB_EP1IN
#define USB_CDC_DATAOUT_EP          USB_EP1OUT

#define USB_CDC_EPDEV_MAXP          USB_EP1_FIFOSIZE                                                // The same for EP2
#define USB_CDC_EPCTL_MAXP          USB_EP3_FIFOSIZE

#define CDC_CTL_INTERFACE_INDEX     0x00
#define CDC_DATA_INTERFACE_INDEX    0x01

#define CDC_OUTBUF_MIN_SIZE         (2 * USB_CDC_EPDEV_MAXP)

#define CDC_TRANSMITTIMEOUT         500

/* Used CDC interface requests */
#define RING_AUX_JACK               0x15
#define SET_LINE_CODING             0x20
#define GET_LINE_CODING             0x21
#define SET_CONTROL_LINE_STATE      0x22
#define LINE_DTR                    (1 << 0)
#define LINE_RTS                    (1 << 1)

typedef struct tag_CDC_VENDOR_REQ
{
    uint16_t Request;
    uint8_t  Data;
} CDC_VENDOR_REQ;

typedef struct tag_CDC_LINE_CODING
{
    uint32_t dwDTERate;                                                                             // Baud rate
    uint8_t  bCharFormat;                                                                           // Stop bits:   0 - 1 Stop bit, 1 - 1.5 Stop bits, 2 - 2 Stop bits
    uint8_t  bParityType;                                                                           // Parity:      0 - None, 1 - Odd, 2 - even, 3 - Mark, 4 - Space
    uint8_t  bDataBits;                                                                             // 5, 6, 7, 8 or 16 bits
} CDC_LINE_CODING, *pCDC_LINE_CODING;

_USB_STRING_(sd00_cdc, 0x0409);
_USB_STRING_(sd01_cdc, u"Prolific Technology Inc. ");
_USB_STRING_(sd02_cdc, u"USB-Serial Controller D");

static const pUSB_STR_DESCR STR_DATA_CDC[] =
{
    (void *)&sd00_cdc,
    (void *)&sd01_cdc,
    (void *)&sd02_cdc
};

static const TUSB_DEV_DESCR DEV_DESC_CDC =
{
    DEV_LENGTH,                                                                                     // Size of this descriptor in bytes
    USB_DEVICE,                                                                                     // DEVICE Descriptor
    0x0110,                                                                                         // USB version 1.1
    0x00,                                                                                           // Device Class Code
    0x00,                                                                                           // Device Subclass Code
    0x00,                                                                                           // Protocol Code
    USB_EP0_FIFOSIZE,                                                                               // EP0 packet max size
    0x067B,                                                                                         // Vendor ID
    0x2303,                                                                                         // Product ID
    0x0400,                                                                                         // Revision ID in BCD
    0x01,                                                                                           // Index of string descriptor describing manufacturer
    0x02,                                                                                           // Index of Product description string in String Descriptor
    0x00,                                                                                           // Index of string descriptor describing the device's serial number
    0x01                                                                                            // Number of possible configurations
};

static const TUSB_CFG_DESCR CFG_DESC_CDC =
{
    CFG_LENGTH,                                                                                     // Size of this descriptor in bytes
    USB_CONFIG,                                                                                     // CONFIGURATION Descriptor
    0x0027,                                                                                         // Total size of descriptor, including Interface and Endpoint descriptors
    0x01,                                                                                           // Number of interfaces supported by this configuration
    0x01,                                                                                           // Index of configuration
    0x00,                                                                                           // Index of string descriptor describing this configuration
    0x80,                                                                                           // Powered from USB
    0x64,                                                                                           // Max power consumption 200 mA
    {
// CDC control interface
        /*Interface Descriptor */
        INT_LENGTH,                                                                                 // Size of this descriptor in bytes
        USB_INTERFACE,                                                                              // INTERFACE Descriptor
        CDC_CTL_INTERFACE_INDEX,                                                                    // Index of interface in this Configuration
        0x00,                                                                                       // Alternative interface ('0' - None)
        0x03,                                                                                       // Number of endpoints, used by this interface
        0xFF,                                                                                       // Class Code: Vendor specific (?)
        0x00,                                                                                       // Subclass Code: Reserved (?)
        0x00,                                                                                       // Protocol Code: No class specific protocol required
        0x00,                                                                                       // Index of string descriptor describing this interface

        // Pipe 1 (endpoint 1)
        END_LENGTH,                                                                                 // Size of this descriptor in bytes
        USB_ENDPOINT,                                                                               // ENDPOINT Descriptor
        USB_EPENUM2INDEX(USB_CDC_CONTROL_EP) | USB_DIR_IN,                                          // IN Endpoint with address 0x03
        USB_EPTYPE_INTR,                                                                            // Data transfer type - Interrupt
        USB_CDC_EPCTL_MAXP, 0x00,                                                                   // Max packet size = 16
        0x01,                                                                                       // Endpoint polling interval - 1 ms

        // Pipe 2 (endpoint 2)
        END_LENGTH,                                                                                 // Size of this descriptor in bytes
        USB_ENDPOINT,                                                                               // ENDPOINT Descriptor
        USB_EPENUM2INDEX(USB_CDC_DATAIN_EP) | USB_DIR_IN,                                           // IN Endpoint with address 0x01
        USB_EPTYPE_BULK,                                                                            // Data transfer Type - Bulk
        USB_CDC_EPDEV_MAXP, 0x00,                                                                   // Max packet size = 64
        0x00,                                                                                       // '0' - endpoint never NAKs

        // Pipe 2 (endpoint 3)
        END_LENGTH,                                                                                 // Size of this descriptor in bytes
        USB_ENDPOINT,                                                                               // ENDPOINT Descriptor
        USB_EPENUM2INDEX(USB_CDC_DATAOUT_EP) | USB_DIR_OUT,                                         // OUT Endpoint with address 0x02 //-V501
        USB_EPTYPE_BULK,                                                                            // Data transfer Type - Bulk
        USB_CDC_EPDEV_MAXP, 0x00,                                                                   // Max packet size = 64
        0x00                                                                                        // '0' - endpoint never NAKs
    }
};

static const CDC_VENDOR_REQ VReq[] =
{
    {0x0080, 0x01},
    {0x0081, 0x00},
    {0x0082, 0x44},
    {0x8383, 0xFF},
    {0x8484, 0x02},
    {0x8686, 0xAA},
    {0x9494, 0x00}
};

static TUSBDRIVERINTERFACE USB_CDC_Interface;
static pCDCEVENTER IntEventerInfo;
static uint8_t  CDC_DeviceConfig;
static uint16_t CDC_DeviceStatus;
static volatile boolean USB_CDC_Connected, USB_CDC_WaitTXAck;
static volatile boolean USB_CDC_TXTimeout, CDCInterfaceInitialized;
static uint8_t CDC_OUTBuffer[USB_CDC_EPDEV_MAXP];
static pRINGBUF CDC_OUTRingBuffer;
static pTIMER CDC_TimeoutTimer;
static uint32_t CDC_PrevTransmitAmount;

static CDC_LINE_CODING CDC_LineCoding =
{
    115200,
    0,
    0,
    8
};

static uint8_t USB_CDC_GetStringDescriptorCount(void)
{
    return sizeof(STR_DATA_CDC) / sizeof(STR_DATA_CDC[0]);
}

static pUSB_STR_DESCR USB_CDC_GetStringDescriptor(uint8_t Index)
{
    return (Index < USB_CDC_GetStringDescriptorCount()) ? STR_DATA_CDC[Index] : NULL;
}

static void USB_CDC_SetConfiguration(uint8_t Index)
{
    /* Do something here when the configuration changes */
    CDC_DeviceConfig = Index;
}

static void USB_CDC_IntFlashRXBuffer(void)
{
    RB_FlashBuffer(CDC_OUTRingBuffer);
}

static void USB_CDC_IntFlashTXBuffer(void)
{
    if (CDCInterfaceInitialized)
    {
        USB_PrepareDataTransmit(USB_CDC_DATAIN_EP, NULL, 0);
        USB_CDC_WaitTXAck = false;
    }
}

static void USB_CDC_TXTimeoutHandler(pTIMER Timer)
{
    USB_CDC_TXTimeout = true;
    USB_CDC_FlashTXBuffer(IntEventerInfo);
}

static void USB_CDC_RestartTXTimeout(void)
{
    uint32_t intflags = __disable_interrupts();

    LRT_Start(CDC_TimeoutTimer);
    USB_CDC_TXTimeout = false;
    __restore_interrupts(intflags);
}

static void USB_CDC_StopTXTimeout(void)
{
    uint32_t intflags = __disable_interrupts();

    LRT_Stop(CDC_TimeoutTimer);
    USB_CDC_TXTimeout = false;
    __restore_interrupts(intflags);
}

static void USB_CDC_SetConnectedStatus(boolean Connected)
{
    if (USB_CDC_Connected != Connected)
    {
        USB_CDC_Connected = Connected;
        if (!Connected) USB_CDC_StopTXTimeout();
        if ((IntEventerInfo != NULL) && (IntEventerInfo->OnStatusChange != NULL))
            IntEventerInfo->OnStatusChange((Connected) ? CDC_CONNECTED : CDC_DISCONNECTED);
    }
}

static void USB_CDC_ConnectHandler(boolean Connected)
{
    USB_CDC_SetConnectedStatus(Connected);
    USB_CDC_IntFlashRXBuffer();
    USB_CDC_IntFlashTXBuffer();
}

static void USB_CDC_InterfaceReqHandler(pUSBSETUP Setup)
{
    boolean Error = false;

    switch (Setup->bRequest)
    {
    case SET_LINE_CODING:
        if (USB_GetEPStage(USB_EP0) == EPSTAGE_IDLE)
            USB_PrepareDataReceive(USB_EP0, &CDC_LineCoding, min(Setup->wLength, sizeof(CDC_LineCoding)));
        else
        {
            /* Do something here with updated CDC_LineCoding */
            return;
        }
        break;
    case GET_LINE_CODING:
        USB_PrepareDataTransmit(USB_EP0, &CDC_LineCoding, min(Setup->wLength, sizeof(CDC_LineCoding)));
        break;
    case SET_CONTROL_LINE_STATE:
        /* Setup->wValue: bit 0 - DTR value,
                          bit 1 - RTS value
        */
        USB_CDC_ConnectHandler((Setup->wValue & LINE_DTR) != 0);
        break;
    default:
        Error = true;
        break;
    }

    USB_UpdateEPState(USB_EP0, USB_DIR_OUT, Error, USB_GetEPStage(USB_EP0) == EPSTAGE_IDLE);
}

static void USB_CDC_VendorReqHandler(pUSBSETUP Setup)
{
    boolean Error = false;

    if (((Setup->bmRequestType & USB_DIR_MASK) == USB_DIR_IN) &&
            (Setup->wValue != 0x0606))
    {
        uint32_t i;

        Error = true;

        for(i = 0; i < sizeof(VReq) / sizeof(CDC_VENDOR_REQ); i++)
            if (Setup->wValue == VReq[i].Request)
            {
                USB_PrepareDataTransmit(USB_EP0, (void *)&VReq[i].Data, sizeof(VReq[i].Data));
                Error = false;
                break;
            }
    }

    USB_UpdateEPState(USB_EP0, USB_DIR_OUT, Error, USB_GetEPStage(USB_EP0) == EPSTAGE_IDLE);
}

static void USB_CDC_CtlHandler(uint8_t EPAddress)
{
}

static boolean USB_CDC_WaitTXIdle(void)
{
    USB_CDC_RestartTXTimeout();
    while(USB_GetEPStage(USB_CDC_DATAIN_EP) != EPSTAGE_IDLE) {}

    /* Checking whether the connection is active */
    if (!USB_CDC_Connected) return true;

    /* Timeout check */
    if (USB_CDC_TXTimeout)
    {
        if (IntEventerInfo->OnStatusChange != NULL)
            IntEventerInfo->OnStatusChange(CDC_TXTIMEOUT);
        return true;
    }
    return false;
}

static void USB_CDC_DataHandler(uint8_t EPAddress)
{
    if (EPAddress == (USB_EPENUM2INDEX(USB_CDC_DATAIN_EP) | USB_DIR_IN))
    {
        if (USB_GetEPStage(USB_CDC_DATAIN_EP) == EPSTAGE_IN)
        {
            uint32_t CurrentlyTransmitted = USB_GetDataAmount(USB_CDC_DATAIN_EP) - CDC_PrevTransmitAmount;

            CDC_PrevTransmitAmount = USB_GetDataAmount(USB_CDC_DATAIN_EP);

            if (USB_CDC_WaitTXAck)
            {
                USB_SetEPStage(USB_CDC_DATAIN_EP, EPSTAGE_IDLE);
                USB_CDC_StopTXTimeout();
                USB_CDC_WaitTXAck = false;
            }
            else
            {
                if (IntEventerInfo->OnDataTransmitted != NULL)
                {
                    USB_CDC_StopTXTimeout();
                    IntEventerInfo->OnDataTransmitted(CurrentlyTransmitted);
                }
                USB_CDC_WaitTXAck = USB_DataTransmit(USB_CDC_DATAIN_EP);
                USB_CDC_RestartTXTimeout();
            }
        }
    }
    else if (EPAddress == (USB_EPENUM2INDEX(USB_CDC_DATAOUT_EP) | USB_DIR_OUT)) //-V501
    {
        uint32_t ReceivedCount;

        USB_DataReceive(USB_CDC_DATAOUT_EP);
        if (USB_CDC_Connected)
        {
            if ((ReceivedCount = USB_GetDataAmount(USB_CDC_DATAOUT_EP)) != 0)
                RB_WriteData(CDC_OUTRingBuffer, CDC_OUTBuffer, ReceivedCount);

            if (((ReceivedCount = RB_GetCurrentDataCount(CDC_OUTRingBuffer)) != 0) &&
                    (IntEventerInfo->OnDataReceived != NULL))
                IntEventerInfo->OnDataReceived(ReceivedCount);
        }
        USB_PrepareDataReceive(USB_CDC_DATAOUT_EP, CDC_OUTBuffer, sizeof(CDC_OUTBuffer));
    }
}

void *USB_CDC_Initialize(void)
{
    CDCInterfaceInitialized  = false;
    CDC_DeviceConfig = 0;
    CDC_DeviceStatus = 0;
    USB_CDC_ConnectHandler(false);
    memset(&USB_CDC_Interface, 0x00, sizeof(TUSBDRIVERINTERFACE));

    if (USB_GetDeviceState() == USB_DEVICE_OFF)
    {
        LRT_Destroy(CDC_TimeoutTimer);
        return NULL;
    }

    if (CDC_TimeoutTimer == NULL)
        CDC_TimeoutTimer = LRT_Create(CDC_TRANSMITTIMEOUT, USB_CDC_TXTimeoutHandler, TF_DIRECT);
    if (CDC_TimeoutTimer == NULL) return NULL;

    USB_CDC_Interface.DeviceDescriptor = (pUSB_DEV_DESCR)&DEV_DESC_CDC;
    USB_CDC_Interface.ConfigDescriptor = (pUSB_CFG_DESCR)&CFG_DESC_CDC;
    USB_CDC_Interface.GetStringDescriptor = USB_CDC_GetStringDescriptor;
    USB_CDC_Interface.ConfigIndex = &CDC_DeviceConfig;
    USB_CDC_Interface.SetConfiguration = USB_CDC_SetConfiguration;
    USB_CDC_Interface.DeviceStatus = &CDC_DeviceStatus;
    USB_CDC_Interface.InterfaceReqHandler = USB_CDC_InterfaceReqHandler;
    USB_CDC_Interface.VendorReqHandler = USB_CDC_VendorReqHandler;

    /* Configure CDC control endpoint */
    USB_SetupEndpoint(USB_CDC_CONTROL_EP, USB_CDC_CtlHandler, USB_CDC_EPCTL_MAXP);
    USB_SetEndpointEnabled(USB_CDC_CONTROL_EP, true);
    /* Configure CDC data IN endpoint */
    USB_SetupEndpoint(USB_CDC_DATAIN_EP, USB_CDC_DataHandler, USB_CDC_EPDEV_MAXP);
    USB_SetEndpointEnabled(USB_CDC_DATAIN_EP, true);
    /* Configure CDC data OUT endpoint */
    USB_SetupEndpoint(USB_CDC_DATAOUT_EP, USB_CDC_DataHandler, USB_CDC_EPDEV_MAXP);
    USB_PrepareDataReceive(USB_CDC_DATAOUT_EP, CDC_OUTBuffer, sizeof(CDC_OUTBuffer));
    USB_SetEndpointEnabled(USB_CDC_DATAOUT_EP, true);

    CDCInterfaceInitialized = true;

    return &USB_CDC_Interface;
}

TCDCSTATUS USB_CDC_Open(pCDCEVENTER EventerInfo)
{
    if ((EventerInfo != NULL) && (IntEventerInfo == NULL) && CDCInterfaceInitialized)
    {
        pRINGBUF tmpRingBuffer;
        uint32_t RingBufferSize = max(EventerInfo->OutBufferSize, CDC_OUTBUF_MIN_SIZE);

        if (CDC_OUTRingBuffer == NULL) tmpRingBuffer = RB_Create(RingBufferSize);
        else if (CDC_OUTRingBuffer->BufferSize != RingBufferSize)
        {
            uint32_t intflags = __disable_interrupts();

            CDC_OUTRingBuffer = RB_Destroy(CDC_OUTRingBuffer);
            __restore_interrupts(intflags);
            tmpRingBuffer = RB_Create(RingBufferSize);
        }
        else tmpRingBuffer = CDC_OUTRingBuffer;

        if (tmpRingBuffer != NULL)
        {
            uint32_t intflags = __disable_interrupts();

            IntEventerInfo = EventerInfo;
            CDC_OUTRingBuffer = tmpRingBuffer;
            IntEventerInfo->OutBufferSize = CDC_OUTRingBuffer->BufferSize;
            __restore_interrupts(intflags);
            return CDC_OK;
        }
    }
    return CDC_FAILED;
}

TCDCSTATUS USB_CDC_Close(pCDCEVENTER EventerInfo)
{
    if ((EventerInfo != NULL) && (EventerInfo == IntEventerInfo))
    {
        uint32_t intflags = __disable_interrupts();

        USB_CDC_IntFlashTXBuffer();
        CDC_OUTRingBuffer = RB_Destroy(CDC_OUTRingBuffer);
        IntEventerInfo = NULL;
        __restore_interrupts(intflags);

        return CDC_OK;
    }
    return CDC_FAILED;
}

uint32_t USB_CDC_Read(pCDCEVENTER EventerInfo, uint8_t *DataPtr, uint32_t Count)
{
    if ((EventerInfo != NULL) && (EventerInfo == IntEventerInfo) &&
            (DataPtr != NULL) && CDCInterfaceInitialized)
    {
        return RB_ReadData(CDC_OUTRingBuffer, DataPtr, Count);
    }
    return 0;
}

uint32_t USB_CDC_Write(pCDCEVENTER EventerInfo, uint8_t *DataPtr, uint32_t Count)
{
    uint32_t WCount = 0;

    if (USB_CDC_Connected && (DataPtr != NULL) && Count && CDCInterfaceInitialized &&
            (EventerInfo != NULL) && (EventerInfo == IntEventerInfo))
        do
        {
            if (USB_CDC_WaitTXIdle()) break;

            CDC_PrevTransmitAmount = 0;
            USB_PrepareDataTransmit(USB_CDC_DATAIN_EP, DataPtr, Count);
            USB_DataTransmit(USB_CDC_DATAIN_EP);

            if (USB_CDC_WaitTXIdle()) break;

            WCount = Count;
        }
        while(0);

    return WCount;
}

TCDCSTATUS USB_CDC_FlashRXBuffer(pCDCEVENTER EventerInfo)
{
    if ((EventerInfo != NULL) && (EventerInfo == IntEventerInfo) && CDCInterfaceInitialized)
    {
        uint32_t intflags = __disable_interrupts();

        USB_CDC_IntFlashRXBuffer();
        __restore_interrupts(intflags);
        return CDC_OK;
    }
    return CDC_FAILED;
}

TCDCSTATUS USB_CDC_FlashTXBuffer(pCDCEVENTER EventerInfo)
{
    if ((EventerInfo != NULL) && (EventerInfo == IntEventerInfo) && CDCInterfaceInitialized)
    {
        uint32_t intflags = __disable_interrupts();

        USB_CDC_IntFlashTXBuffer();
        __restore_interrupts(intflags);
        return CDC_OK;
    }
    return CDC_FAILED;
}
