#include "usb.h"
#include "pci.h"

static pci_device_t* controller = 0;

int usb_init() {
    controller = pci_find_class(USB_PCI_CLASS, USB_PCI_SUBCLASS);
    return controller ? 1 : 0;
}

uint8_t usb_has_controller() {
    return controller ? 1 : 0;
}

const char* usb_controller_type() {
    if (!controller) return "none";
    switch (controller->prog_if) {
        case USB_UHCI: return "UHCI (USB 1.1)";
        case USB_OHCI: return "OHCI (USB 1.1)";
        case USB_EHCI: return "EHCI (USB 2.0)";
        case USB_XHCI: return "xHCI (USB 3.0)";
        default:       return "Unknown USB controller";
    }
}
