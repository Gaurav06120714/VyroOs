#include "xhci.h"
#include "pci.h"
#include "heap.h"

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

// Allocate `bytes` aligned to `align`. Slack-allocate and round up. Returns
// the aligned pointer; the underlying allocation is leaked (xHCI is set up
// once at boot and never torn down).
static void* xhci_alloc_aligned(uint32_t bytes, uint32_t align) {
    uint8_t* raw = (uint8_t*)kmalloc(bytes + align);
    if (!raw) return 0;
    uint64_t addr = (uint64_t)(uintptr_t)raw;
    addr = (addr + align - 1) & ~(uint64_t)(align - 1);
    return (void*)(uintptr_t)addr;
}

#define XHCI_TRB_BYTES   16
#define XHCI_CMD_RING_N 256

static uint64_t dcbaa_phys     = 0;
static uint64_t cmd_ring_phys  = 0;

int xhci_bring_up(void) {
    if (!info.present) return 0;
    uint64_t oper = info.mmio_base + info.caplength;

    // 1. Enable all device slots.
    uint32_t cfg = mmio_r32(oper + XHCI_OP_CONFIG);
    cfg = (cfg & ~0xFFu) | (info.max_slots & 0xFF);
    mmio_w32(oper + XHCI_OP_CONFIG, cfg);

    // 2. Allocate DCBAA (max_slots + 1 entries, each 8 bytes). 64-byte aligned.
    uint32_t dcbaa_bytes = (info.max_slots + 1) * 8;
    void* dcbaa = xhci_alloc_aligned(dcbaa_bytes, 64);
    if (!dcbaa) return 0;
    for (uint32_t i = 0; i < dcbaa_bytes; i++) ((uint8_t*)dcbaa)[i] = 0;
    dcbaa_phys = (uint64_t)(uintptr_t)dcbaa;
    // Write 64-bit DCBAAP (low then high).
    mmio_w32(oper + XHCI_OP_DCBAAP,     (uint32_t)(dcbaa_phys & 0xFFFFFFFF));
    mmio_w32(oper + XHCI_OP_DCBAAP + 4, (uint32_t)(dcbaa_phys >> 32));

    // 3. Allocate Command Ring (256 TRBs, 16 bytes each = 4 KB). 64-byte aligned.
    void* ring = xhci_alloc_aligned(XHCI_CMD_RING_N * XHCI_TRB_BYTES, 64);
    if (!ring) return 0;
    for (uint32_t i = 0; i < XHCI_CMD_RING_N * XHCI_TRB_BYTES; i++) ((uint8_t*)ring)[i] = 0;
    cmd_ring_phys = (uint64_t)(uintptr_t)ring;
    // CRCR: low DWORD has flags + addr low. RCS=1 (bit 0). Cycle state initially 1.
    uint32_t crcr_lo = (uint32_t)(cmd_ring_phys & 0xFFFFFFC0u) | 0x1;
    uint32_t crcr_hi = (uint32_t)(cmd_ring_phys >> 32);
    mmio_w32(oper + XHCI_OP_CRCR,     crcr_lo);
    mmio_w32(oper + XHCI_OP_CRCR + 4, crcr_hi);

    // 4. Start the controller (USBCMD.RS=1).
    uint32_t cmd = mmio_r32(oper + XHCI_OP_USBCMD);
    mmio_w32(oper + XHCI_OP_USBCMD, cmd | USBCMD_RS);
    // Wait for HCH=0.
    for (int i = 0; i < 1000000; i++) {
        if (!(mmio_r32(oper + XHCI_OP_USBSTS) & USBSTS_HCH)) break;
    }
    return 1;
}

uint64_t xhci_dcbaa_phys(void)    { return dcbaa_phys; }
uint64_t xhci_cmd_ring_phys(void) { return cmd_ring_phys; }

// Event Ring scaffolding. Interrupter 0 (the only one this phase uses)
// lives in the runtime register region. RTSOFF is in the capability registers.
#define XHCI_RT_IR0_ERSTSZ   0x028
#define XHCI_RT_IR0_ERSTBA   0x030
#define XHCI_RT_IR0_ERDP     0x038

static uint64_t event_ring_phys = 0;
static uint64_t erst_phys       = 0;

int xhci_event_ring_setup(void) {
    if (!info.present) return 0;
    uint64_t cap = info.mmio_base;
    // RTSOFF is at offset 0x18 (DWORD-aligned).
    uint32_t rtsoff = mmio_r32(cap + 0x18) & ~0x1F;
    uint64_t runtime = cap + rtsoff;

    // Allocate event ring: 256 TRBs × 16 bytes = 4 KB, 64-byte aligned.
    void* ring = xhci_alloc_aligned(256 * 16, 64);
    if (!ring) return 0;
    for (int i = 0; i < 256 * 16; i++) ((uint8_t*)ring)[i] = 0;
    event_ring_phys = (uint64_t)(uintptr_t)ring;

    // ERST: one entry. Each entry is 16 bytes: ring_addr(8) | size(2) | rsvd(6).
    uint8_t* erst = (uint8_t*)xhci_alloc_aligned(16, 64);
    if (!erst) return 0;
    for (int i = 0; i < 16; i++) erst[i] = 0;
    *(volatile uint64_t*)(erst + 0) = event_ring_phys;
    *(volatile uint16_t*)(erst + 8) = 256;
    erst_phys = (uint64_t)(uintptr_t)erst;

    // Program Interrupter 0
    mmio_w32(runtime + 0x20 + XHCI_RT_IR0_ERSTSZ, 1);            // one segment
    mmio_w32(runtime + 0x20 + XHCI_RT_IR0_ERDP,
             (uint32_t)(event_ring_phys & 0xFFFFFFF0));
    mmio_w32(runtime + 0x20 + XHCI_RT_IR0_ERDP + 4,
             (uint32_t)(event_ring_phys >> 32));
    mmio_w32(runtime + 0x20 + XHCI_RT_IR0_ERSTBA,
             (uint32_t)(erst_phys & 0xFFFFFFC0));
    mmio_w32(runtime + 0x20 + XHCI_RT_IR0_ERSTBA + 4,
             (uint32_t)(erst_phys >> 32));
    return 1;
}

uint64_t xhci_event_ring_phys(void) { return event_ring_phys; }
uint64_t xhci_erst_phys(void)       { return erst_phys; }

// Command-ring state for Enable Slot et al.
static uint32_t cmd_ring_idx   = 0;
static uint8_t  cmd_cycle_bit  = 1;

// DBOFF lives at capability register 0x14.
static uint64_t doorbell_base(void) {
    uint32_t dboff = mmio_r32(info.mmio_base + 0x14) & ~0x3;
    return info.mmio_base + dboff;
}

int xhci_enable_slot(void) {
    if (!info.present || cmd_ring_phys == 0) return 0;
    // TRB layout: 16 bytes — Param[0..7]=0, Status[8..11]=0, Control[12..15]
    // Control: cycle bit(0) | TRB type 9 << 10 | slot_type=0 << 16
    uint8_t* trb = (uint8_t*)(uintptr_t)cmd_ring_phys + cmd_ring_idx * 16;
    for (int i = 0; i < 16; i++) trb[i] = 0;
    uint32_t control = (uint32_t)cmd_cycle_bit | (9u << 10);
    trb[12] = (uint8_t)control;
    trb[13] = (uint8_t)(control >> 8);
    trb[14] = (uint8_t)(control >> 16);
    trb[15] = (uint8_t)(control >> 24);

    cmd_ring_idx++;
    if (cmd_ring_idx >= XHCI_CMD_RING_N - 1) {
        // We'd need a Link TRB to wrap. For this phase just stop short.
        cmd_ring_idx = 0;
        cmd_cycle_bit ^= 1;
    }

    // Ring the host controller doorbell (DB[0]).
    mmio_w32(doorbell_base() + 0, 0);
    return 1;
}

uint32_t xhci_cmd_ring_index(void) { return cmd_ring_idx; }
