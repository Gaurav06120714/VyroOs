#ifndef XHCI_H
#define XHCI_H

#include "../include/types.h"

// xHCI USB 3.0 host-controller scaffolding.
// This phase detects the controller via PCI (class 0x0C, subclass 0x03,
// prog-if 0x30), parses the capability registers, and exposes the basic
// topology fields (number of slots, ports, interrupters, page size).
// Full device enumeration (command ring, slot allocation, port reset,
// USB descriptor read) is a separate future phase — the xHCI spec is
// hundreds of pages and warrants its own dedicated track.

typedef struct {
    int       present;
    uint64_t  mmio_base;
    uint8_t   caplength;
    uint16_t  hci_version;
    uint32_t  hcsparams1;     // device slots, interrupters, ports
    uint32_t  hcsparams2;
    uint32_t  hccparams1;
    uint32_t  page_size_bytes;
    uint32_t  max_slots;
    uint32_t  max_intrs;
    uint32_t  max_ports;
} xhci_info_t;

int  xhci_init(void);                 // returns 1 if controller found
const xhci_info_t* xhci_info(void);

#endif
