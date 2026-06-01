#include "pci.h"
#include "../drivers/pic.h"   // outb/inb + 32-bit port helpers below

#define MAX_PCI_DEVICES 32
static pci_device_t devices[MAX_PCI_DEVICES];
static uint32_t     device_count = 0;

// 32-bit port I/O (PCI config uses dwords)
static inline void outl(uint16_t port, uint32_t val) {
    __asm__ volatile("outl %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint32_t inl(uint16_t port) {
    uint32_t ret;
    __asm__ volatile("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// ─────────────────────────────────────────────────
// pci_config_read: read a dword from PCI config space
// ─────────────────────────────────────────────────
uint32_t pci_config_read(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) |
                                  (offset & 0xFC) | 0x80000000);
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

// ─────────────────────────────────────────────────
// pci_scan: enumerate all PCI buses/slots/functions
// ─────────────────────────────────────────────────
void pci_scan() {
    device_count = 0;

    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            for (uint8_t func = 0; func < 8; func++) {
                uint32_t reg0 = pci_config_read((uint8_t)bus, slot, func, 0x00);
                uint16_t vendor = reg0 & 0xFFFF;
                if (vendor == 0xFFFF) continue;   // no device

                if (device_count >= MAX_PCI_DEVICES) return;

                uint32_t reg2 = pci_config_read((uint8_t)bus, slot, func, 0x08);
                uint32_t bar0 = pci_config_read((uint8_t)bus, slot, func, 0x10);
                uint32_t regF = pci_config_read((uint8_t)bus, slot, func, 0x3C);

                pci_device_t* d = &devices[device_count++];
                d->bus        = (uint8_t)bus;
                d->slot       = slot;
                d->func       = func;
                d->vendor_id  = vendor;
                d->device_id  = (reg0 >> 16) & 0xFFFF;
                d->class_id   = (reg2 >> 24) & 0xFF;
                d->subclass   = (reg2 >> 16) & 0xFF;
                d->prog_if    = (reg2 >> 8) & 0xFF;
                d->bar0       = bar0;
                d->irq_line   = regF & 0xFF;
                d->valid      = 1;

                // Only check func 0 for non-multifunction devices
                if (func == 0) {
                    uint32_t header = pci_config_read((uint8_t)bus, slot, 0, 0x0C);
                    if (!((header >> 16) & 0x80)) break;  // not multifunction
                }
            }
        }
    }
}

uint32_t pci_device_count() { return device_count; }

pci_device_t* pci_get_device(uint32_t index) {
    if (index >= device_count) return 0;
    return &devices[index];
}

// ─────────────────────────────────────────────────
// pci_find_network: find a network controller (class 0x02)
// ─────────────────────────────────────────────────
pci_device_t* pci_find_network() {
    for (uint32_t i = 0; i < device_count; i++) {
        if (devices[i].class_id == 0x02) return &devices[i];
    }
    return 0;
}

pci_device_t* pci_find_class(uint8_t class_id, uint8_t subclass) {
    for (uint32_t i = 0; i < device_count; i++) {
        if (devices[i].class_id == class_id && devices[i].subclass == subclass)
            return &devices[i];
    }
    return 0;
}
