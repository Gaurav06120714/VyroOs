#include "xhci.h"
#include "pci.h"

static xhci_info_t info;

static inline uint32_t mmio_r32(uint64_t addr) { return *(volatile uint32_t*)addr; }
static inline void     mmio_w32(uint64_t addr, uint32_t v) { *(volatile uint32_t*)addr = v; }

// Operational registers offsets (relative to oper_base = mmio_base + caplength)
#define XHCI_OP_USBCMD    0x00
#define XHCI_OP_USBSTS    0x04
#define XHCI_OP_PAGESIZE  0x08
#define XHCI_OP_DNCTRL    0x14
#define XHCI_OP_CRCR      0x18
#define XHCI_OP_DCBAAP    0x30
#define XHCI_OP_CONFIG    0x38

// USBCMD bits
#define USBCMD_RS         (1u << 0)
#define USBCMD_HCRST      (1u << 1)

// USBSTS bits
#define USBSTS_HCH        (1u << 0)
#define USBSTS_CNR        (1u << 11)

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

int xhci_reset(void) {
    if (!info.present) return 0;
    uint64_t oper = info.mmio_base + info.caplength;

    // 1. Halt: clear Run/Stop
    uint32_t cmd = mmio_r32(oper + XHCI_OP_USBCMD);
    mmio_w32(oper + XHCI_OP_USBCMD, cmd & ~USBCMD_RS);
    // Wait for HCH=1
    for (int i = 0; i < 1000000; i++) {
        if (mmio_r32(oper + XHCI_OP_USBSTS) & USBSTS_HCH) break;
    }

    // 2. Reset
    mmio_w32(oper + XHCI_OP_USBCMD, USBCMD_HCRST);
    for (int i = 0; i < 1000000; i++) {
        if (!(mmio_r32(oper + XHCI_OP_USBCMD) & USBCMD_HCRST)) break;
    }
    // 3. Wait for Controller-Not-Ready to clear
    for (int i = 0; i < 1000000; i++) {
        if (!(mmio_r32(oper + XHCI_OP_USBSTS) & USBSTS_CNR)) break;
    }
    return !(mmio_r32(oper + XHCI_OP_USBSTS) & USBSTS_CNR);
}

uint32_t xhci_status(void) {
    if (!info.present) return 0;
    return mmio_r32(info.mmio_base + info.caplength + XHCI_OP_USBSTS);
}
