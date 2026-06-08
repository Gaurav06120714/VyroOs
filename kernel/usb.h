#ifndef USB_H
#define USB_H

#include "../include/types.h"

#define USB_PCI_CLASS    0x0C
#define USB_PCI_SUBCLASS 0x03

#define USB_UHCI 0x00
#define USB_OHCI 0x10
#define USB_EHCI 0x20
#define USB_XHCI 0x30

typedef struct {
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
} __attribute__((packed)) usb_device_descriptor_t;

typedef struct {
    uint8_t  bmRequestType;
    uint8_t  bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} __attribute__((packed)) usb_setup_packet_t;

#define USB_REQ_GET_DESCRIPTOR 6
#define USB_REQ_SET_ADDRESS    5
#define USB_REQ_SET_CONFIG     9

int         usb_init();
const char* usb_controller_type();
uint8_t     usb_has_controller();

#endif
