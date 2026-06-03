#ifndef AHCI_H
#define AHCI_H

#include "../include/types.h"

// AHCI (Advanced Host Controller Interface) for SATA. Detects an AHCI HBA
// via PCI (class 0x01, subclass 0x06, prog-if 0x01), reads ABAR (BAR5),
// parses capabilities, and exposes the per-port presence map. Real disk
// I/O via FIS-based command lists is a follow-up phase — this scaffolding
// gives us the controller knowledge.

typedef struct {
    int       present;
    uint64_t  mmio_base;       // ABAR
    uint32_t  capabilities;
    uint32_t  port_implemented_mask;
    uint32_t  version;
    uint32_t  num_ports;        // CAP.NP + 1
    uint32_t  num_command_slots;// CAP.NCS + 1
    uint8_t   supports_64bit;
} ahci_info_t;

int  ahci_init(void);
const ahci_info_t* ahci_info(void);

// Per-port status: bit set in returned mask = device present + active.
uint32_t ahci_active_ports(void);

#endif
