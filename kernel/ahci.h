#ifndef AHCI_H
#define AHCI_H

#include "../include/types.h"

// AHCI (Advanced Host Controller Interface) for SATA. Detects an AHCI HBA
// via PCI (class 0x01, subclass 0x06, prog-if 0x01), reads ABAR (BAR5),
// parses capabilities, and exposes the per-port presence map.
//
// vC.6.1 adds command-list + FIS-based sector read on the first active
// port (single-sector PIO read via ATA READ_DMA_EX command 0x25).

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

// vC.6.1: bring port up (allocate CLB/FB, spin up, start FIS RX + cmd engine).
// Returns 1 on success, 0 if no active device on that port.
int ahci_port_init(uint32_t port);

// vC.6.1: read `count` sectors starting at LBA `lba` from `port` into `buf`.
// Uses ATA READ_DMA_EX (0x25) via slot 0. Returns 1 on success.
int ahci_port_read(uint32_t port, uint64_t lba, uint32_t count, void *buf);

#endif
