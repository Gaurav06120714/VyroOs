#ifndef PCI_H
#define PCI_H

#include "../include/types.h"

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

typedef struct {
    uint8_t  bus;
    uint8_t  slot;
    uint8_t  func;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t  class_id;
    uint8_t  subclass;
    uint8_t  prog_if;
    uint32_t bar0;
    uint8_t  irq_line;
    uint8_t  valid;
} pci_device_t;

uint32_t pci_config_read(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
void     pci_scan();
uint32_t pci_device_count();
pci_device_t* pci_get_device(uint32_t index);
pci_device_t* pci_find_network();
pci_device_t* pci_find_class(uint8_t class_id, uint8_t subclass);

#endif
