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
#ifndef _USB9_H_
#define _USB9_H_

#define USB_EPNUM_MASK              0x000F

#define DEV_LENGTH                  18                                                              // Length of Device Descriptor
#define CFG_LENGTH                  9                                                               // Length of Configuration Descriptor
#define INT_LENGTH                  9                                                               // Length of Interface Descriptor
#define HID_LENGTH                  9                                                               // Length of HID Descriptor
#define END_LENGTH                  7                                                               // Length of Endpoint Descriptor

// Standard Request Codes
#define USB_GET_STATUS              0x00
#define USB_CLEAR_FEATURE           0x01
#define USB_SET_FEATURE             0x03
#define USB_SET_ADDRESS             0x05
#define USB_GET_DESCRIPTOR          0x06
#define USB_SET_DESCRIPTOR          0x07
#define USB_GET_CONFIGURATION       0x08
#define USB_SET_CONFIGURATION       0x09
#define USB_GET_INTERFACE           0x0A
#define USB_SET_INTERFACE           0x0B
#define USB_SYNCH_FRAME             0x0C

// Feature selectors
#define USB_FS_DEVICE               0x00
#define USB_FS_INTERFACE            0x01
#define USB_FS_ENDPOINT             0x02
#define USB_FS_MASK                 0x03

// Request Type Field
#define USB_CMD_TYPEMASK            0x60
#define USB_CMD_STDREQ              0x00
#define USB_CMD_CLASSREQ            0x20
#define USB_CMD_VENDREQ             0x40
#define USB_CMD_STDDEVIN            0x80
#define USB_CMD_STDDEVOUT           0x00
#define USB_CMD_STDIFIN             0x81
#define USB_CMD_STDIFOUT            0x01
#define USB_CMD_STDEPIN             0x82
#define USB_CMD_STDEPOUT            0x02
#define USB_CMD_CLASSIFIN           0xA1
#define USB_CMD_CLASSIFOUT          0x21

// Standard command descriptor type
#define USB_DEVICE                  0x01
#define USB_CONFIG                  0x02
#define USB_STRING                  0x03
#define USB_INTERFACE               0x04
#define USB_ENDPOINT                0x05
#define USB_DEVICE_QUALIFIER        0x06
#define USB_OTHER_SPEED             0x07
#define USB_INTERFACE_POWER         0x08
#define USB_OTG_DESC                0x09
#define USB_INTERFACE_ASSOCIATION   0x0B
#define HID                         0x21
#define HIDREPORT                   0x22
#define HIDPHYSICAL                 0x23
#define USB_CS_INTERFACE            0x24

#define USB_CMD_DESCMASK            0xFF00
#define USB_CMD_DESCIDXMASK         0x00FF
#define USB_CMD_DEVICE              (USB_DEVICE << 8)
#define USB_CMD_CONFIG              (USB_CONFIG << 8)
#define USB_CMD_STRING              (USB_STRING << 8)
#define USB_CMD_INTERFACE           (USB_INTERFACE << 8)
#define USB_CMD_ENDPOINT            (USB_ENDPOINT << 8)
#define USB_CMD_DEVICE_QUALIFIER    (USB_DEVICE_QUALIFIER << 8)
#define USB_CMD_OTHER_SPEED         (USB_OTHER_SPEED << 8)
#define USB_CMD_INTERFACE_POWER     (USB_INTERFACE_POWER << 8)

// Standard Device Feature Selectors
#define USB_FTR_DEVREMWAKE          0x01
#define USB_FTR_EPHALT              0x00
#define USB_FTR_B_HNP_ENB           0x03
#define USB_FTR_A_HNP_SUPPORT       0x04
#define USB_FTR_A_ALT_HNP_SUPPORT   0x005
#define USB_FTR_TEST_MODE           0x02

// Class codes
#define HIDCLASS                    0x03
#define MASSTORAGECLASS             0x08
#define CDCCLASS                    0x0A
#define NOSUBCLASS                  0x00
#define BOOTSUBCLASS                0x01
#define VENDORSPEC                  0xFF

// Endpoint transfer types
#define USB_EPTYPE_CONTROL          0x00
#define USB_EPTYPE_ISO              0x01
#define USB_EPTYPE_BULK             0x02
#define USB_EPTYPE_INTR             0x03

typedef struct tag_USBSETUP
{
    uint8_t  bmRequestType;
    uint8_t  bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} __packed TUSBSETUP, *pUSBSETUP;

// standard device descriptor
typedef struct tag_USB_DEV_DESCR
{
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t bcdUSB;
    uint8_t  bDeviceClass;
    uint8_t  bDeviceSubClass;
    uint8_t  bDeviceProtocol;
    uint8_t  bMaxPacketSize0;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t  iManufacturer;
    uint8_t  iProduct;
    uint8_t  iSerialNumber;
    uint8_t  bNumConfigurations;
} TUSB_DEV_DESCR, *pUSB_DEV_DESCR;

// standard configuration descriptor
typedef struct tag_USB_CFG_DESCR
{
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t wTotalLength;
    uint8_t  bNumInterfaces;
    uint8_t  bConfigurationValue;
    uint8_t  iConfiguration;
    uint8_t  bmAttributes;
    uint8_t  bMaxPower;
    uint8_t  ExtraData[];
} TUSB_CFG_DESCR, *pUSB_CFG_DESCR;

// standard string descriptor
typedef struct tag_USB_STR_DESCR
{
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t wString[];
} TUSB_STR_DESCR, *pUSB_STR_DESCR;

#define _USB_STRING_(name, str)\
const struct name\
{\
    uint8_t  Length;\
    uint8_t  DescriptorType;\
    uint16_t String[(sizeof(str) - 2) / 2];\
}\
name = {sizeof(name), USB_STRING, str};

typedef struct tag_USBDRIVERINTERFACE
{
    pUSB_DEV_DESCR DeviceDescriptor;
    pUSB_CFG_DESCR ConfigDescriptor;
    pUSB_STR_DESCR (*GetStringDescriptor)(uint8_t Index);
    const uint8_t  *ConfigIndex;
    void           (*SetConfiguration)(uint8_t Index);
    uint8_t        (*(*GetAltInterface)(uint8_t Index));
    boolean        (*SetAltInterface)(uint8_t Index, uint8_t AltIndex);
    const uint16_t *DeviceStatus;
    void           (*InterfaceReqHandler)(pUSBSETUP Setup);
    void           (*VendorReqHandler)(pUSBSETUP Setup);
} TUSBDRIVERINTERFACE, *pUSBDRIVERINTERFACE;

extern boolean USB9_InterfaceInitialize(void);
extern void USB9_HandleSetupRequest(pUSBSETUP Setup);

#endif /* _USB9_H_ */
