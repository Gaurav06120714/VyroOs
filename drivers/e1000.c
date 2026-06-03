#include "e1000.h"
#include "../kernel/pci.h"

static e1000_info_t info;

static inline uint32_t mmio_r32(uint64_t addr) { return *(volatile uint32_t*)addr; }
static inline void     mmio_w32(uint64_t addr, uint32_t v) { *(volatile uint32_t*)addr = v; }

// Known Intel Gigabit Ethernet device IDs that share enough of the 8254x
// programming model to be probed identically.
static int is_intel_eth(uint16_t did) {
    static const uint16_t ids[] = {
        0x100E,   // 82540EM (QEMU default e1000, also VirtualBox)
        0x100F,   // 82545EM
        0x10D3,   // 82574L (laptop)
        0x10EA,   // 82577LM
        0x153A,   // I217-LM
        0x153B,   // I217-V
        0x15A2,   // I218-LM
        0x15B7,   // I219-LM
        0x15B8,   // I219-V
        0x15D7,   // I219-LM (2)
        0x15F2,   // I225-LM (2.5G)
        0x1533    // I210
    };
    for (uint32_t i = 0; i < sizeof(ids)/sizeof(ids[0]); i++)
        if (ids[i] == did) return 1;
    return 0;
}

int e1000_init(void) {
    for (uint32_t i = 0; i < sizeof(info); i++) ((uint8_t*)&info)[i] = 0;

    uint32_t n = pci_device_count();
    for (uint32_t i = 0; i < n; i++) {
        pci_device_t* d = pci_get_device(i);
        if (d->vendor_id != 0x8086) continue;
        if (!is_intel_eth(d->device_id)) continue;
        info.device_id = d->device_id;
        info.mmio_base = d->bar0 & ~0xFULL;
        break;
    }
    if (!info.mmio_base) return 0;

    // Read MAC from RAL/RAH
    uint32_t lo = mmio_r32(info.mmio_base + E1000_REG_RAL);
    uint32_t hi = mmio_r32(info.mmio_base + E1000_REG_RAH);
    info.mac[0] = (lo      ) & 0xFF;
    info.mac[1] = (lo >>  8) & 0xFF;
    info.mac[2] = (lo >> 16) & 0xFF;
    info.mac[3] = (lo >> 24) & 0xFF;
    info.mac[4] = (hi      ) & 0xFF;
    info.mac[5] = (hi >>  8) & 0xFF;

    info.link_status = mmio_r32(info.mmio_base + E1000_REG_STATUS);
    info.present     = 1;
    return 1;
}

const e1000_info_t* e1000_info(void) { return &info; }
