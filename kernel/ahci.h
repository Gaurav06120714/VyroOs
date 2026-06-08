#ifndef AHCI_H
#define AHCI_H

#include "../include/types.h"

typedef struct {
    int       present;
    uint64_t  mmio_base;
    uint32_t  capabilities;
    uint32_t  port_implemented_mask;
    uint32_t  version;
    uint32_t  num_ports;
    uint32_t  num_command_slots;
    uint8_t   supports_64bit;
} ahci_info_t;

int  ahci_init(void);
const ahci_info_t* ahci_info(void);

uint32_t ahci_active_ports(void);

int ahci_port_init(uint32_t port);

int ahci_port_read(uint32_t port, uint64_t lba, uint32_t count, void *buf);

int ahci_port_write(uint32_t port, uint64_t lba, uint32_t count, const void *buf);

int ahci_port_identify(uint32_t port, char *out_model, char *out_serial,
                       uint64_t *out_total_sectors);

#endif
