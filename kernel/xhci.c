#include "xhci.h"
#include "pci.h"

static xhci_info_t info;

static inline uint32_t mmio_r32(uint64_t addr) { return *(volatile uint32_t*)addr; }

int xhci_init(void) {
    for (uint32_t i = 0; i < sizeof(info); i++) ((uint8_t*)&info)[i] = 0;

    pci_device_t* dev = pci_find_class(0x0C, 0x03);
    if (!dev) return 0;
    if (dev->prog_if != 0x30) return 0;       // not xHCI (could be UHCI/OHCI/EHCI)
    info.mmio_base = dev->bar0 & ~0xFULL;
    if (!info.mmio_base) return 0;

    // Capability registers live at MMIO base.
    uint32_t cap0 = mmio_r32(info.mmio_base + 0x00);
    info.caplength   = cap0 & 0xFF;
    info.hci_version = (uint16_t)(cap0 >> 16);
    info.hcsparams1  = mmio_r32(info.mmio_base + 0x04);
    info.hcsparams2  = mmio_r32(info.mmio_base + 0x08);
    info.hccparams1  = mmio_r32(info.mmio_base + 0x10);

    info.max_slots = info.hcsparams1 & 0xFF;
    info.max_intrs = (info.hcsparams1 >> 8) & 0x7FF;
    info.max_ports = (info.hcsparams1 >> 24) & 0xFF;

    // Operational PAGESIZE register at OperBase + 0x08 (one-hot, page = 1<<(bit + 12))
    uint64_t oper_base = info.mmio_base + info.caplength;
    uint32_t ps = mmio_r32(oper_base + 0x08) & 0xFFFF;
    uint32_t bit = 0;
    while (ps && !(ps & 1)) { ps >>= 1; bit++; }
    info.page_size_bytes = ps ? (1u << (bit + 12)) : 4096;

    info.present = 1;
    return 1;
}

const xhci_info_t* xhci_info(void) { return &info; }
