#include "nvme.h"
#include "pci.h"

static nvme_info_t info;

static inline uint64_t mmio_r64(uint64_t addr) {
    uint64_t lo = *(volatile uint32_t*)addr;
    uint64_t hi = *(volatile uint32_t*)(addr + 4);
    return (hi << 32) | lo;
}
static inline uint32_t mmio_r32(uint64_t addr) { return *(volatile uint32_t*)addr; }

int nvme_init(void) {
    for (uint32_t i = 0; i < sizeof(info); i++) ((uint8_t*)&info)[i] = 0;

    // PCI class 0x01 (Mass Storage), subclass 0x08 (NVM), prog-if 0x02 (NVMe).
    pci_device_t* dev = pci_find_class(0x01, 0x08);
    if (!dev || dev->prog_if != 0x02) return 0;

    extern uint32_t pci_config_read(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off);
    uint32_t bar0_lo = pci_config_read(dev->bus, dev->slot, dev->func, 0x10);
    uint32_t bar0_hi = pci_config_read(dev->bus, dev->slot, dev->func, 0x14);
    // 64-bit BAR if bits 1:2 == 10b
    uint64_t base;
    if ((bar0_lo & 0x6) == 0x4) base = ((uint64_t)bar0_hi << 32) | (bar0_lo & ~0xFu);
    else                        base = bar0_lo & ~0xFu;
    if (!base) return 0;

    info.mmio_base       = base;
    info.cap             = mmio_r64(base + 0x00);
    info.version         = mmio_r32(base + 0x08);
    info.max_q_entries   = (info.cap & 0xFFFF) + 1;
    info.doorbell_stride = (info.cap >> 32) & 0xF;
    info.present         = 1;
    return 1;
}

const nvme_info_t* nvme_info(void) { return &info; }
