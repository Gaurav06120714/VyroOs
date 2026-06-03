#include "ahci.h"
#include "pci.h"

static ahci_info_t info;

// --- HBA registers ---
#define HBA_CAP     0x00
#define HBA_GHC     0x04
#define HBA_IS      0x08
#define HBA_PI      0x0C
#define HBA_VS      0x10
#define HBA_PORT0   0x100
#define HBA_PORTSIZE 0x80

// --- Per-port registers (offsets from port base) ---
#define PORT_CLB    0x00
#define PORT_CLBU   0x04
#define PORT_FB     0x08
#define PORT_FBU    0x0C
#define PORT_IS     0x10
#define PORT_IE     0x14
#define PORT_CMD    0x18
#define PORT_TFD    0x20
#define PORT_SIG    0x24
#define PORT_SSTS   0x28
#define PORT_SCTL   0x2C
#define PORT_SERR   0x30
#define PORT_CI     0x38

#define GHC_AE      (1u << 31)   // AHCI enable
#define PXCMD_ST    (1u << 0)
#define PXCMD_FRE   (1u << 4)
#define PXCMD_FR    (1u << 14)
#define PXCMD_CR    (1u << 15)

#define PXTFD_BSY   (1u << 7)
#define PXTFD_DRQ   (1u << 3)

#define FIS_TYPE_REG_H2D  0x27

// --- Command structures (volatile, MMIO-backed) ---
typedef struct __attribute__((packed)) {
    uint16_t flags;            // CFL/A/W/P/R/B/C/PMP/RSV(rsv field per spec)
    uint16_t prdtl;            // PRD table length
    volatile uint32_t prdbc;   // PRD byte count transferred
    uint32_t ctba;             // command table base lo
    uint32_t ctbau;            // command table base hi
    uint32_t rsv[4];
} hba_cmd_header_t;

typedef struct __attribute__((packed)) {
    uint32_t dba;
    uint32_t dbau;
    uint32_t rsv0;
    uint32_t dbc;              // byte count, bit 31 = interrupt-on-completion
} hba_prdt_entry_t;

typedef struct __attribute__((packed)) {
    uint8_t  cfis[64];
    uint8_t  acmd[16];
    uint8_t  rsv[48];
    hba_prdt_entry_t prdt[1];  // single PRD entry for vC.6.1
} hba_cmd_table_t;

// Static allocations (1 KiB aligned for CLB; 256-byte for FB). Reserved
// in the kernel image's BSS — keep it small because the BSS lives in the
// limited 384 KB kernel sector budget. One port for now.
static __attribute__((aligned(1024)))  hba_cmd_header_t g_clb[32];
static __attribute__((aligned(256)))   uint8_t           g_fb[256];
static __attribute__((aligned(128)))   hba_cmd_table_t   g_cmdtbl;

// --- helpers ---
static inline uint32_t mmio_r32(uint64_t addr) { return *(volatile uint32_t*)addr; }
static inline void     mmio_w32(uint64_t addr, uint32_t v) { *(volatile uint32_t*)addr = v; }

static inline uint64_t port_base(uint32_t port) {
    return info.mmio_base + HBA_PORT0 + (uint64_t)port * HBA_PORTSIZE;
}

int ahci_init(void) {
    for (uint32_t i = 0; i < sizeof(info); i++) ((uint8_t*)&info)[i] = 0;

    pci_device_t* dev = pci_find_class(0x01, 0x06);
    if (!dev) return 0;
    if (dev->prog_if != 0x01) return 0;

    extern uint32_t pci_config_read(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off);
    uint32_t bar5 = pci_config_read(dev->bus, dev->slot, dev->func, 0x24);
    info.mmio_base = bar5 & ~0xFULL;
    if (!info.mmio_base) return 0;

    uint32_t ghc = mmio_r32(info.mmio_base + HBA_GHC);
    mmio_w32(info.mmio_base + HBA_GHC, ghc | GHC_AE);

    info.capabilities          = mmio_r32(info.mmio_base + HBA_CAP);
    info.port_implemented_mask = mmio_r32(info.mmio_base + HBA_PI);
    info.version               = mmio_r32(info.mmio_base + HBA_VS);
    info.num_ports             = ((info.capabilities >> 0)  & 0x1F) + 1;
    info.num_command_slots     = ((info.capabilities >> 8)  & 0x1F) + 1;
    info.supports_64bit        =  (info.capabilities >> 31) & 1;
    info.present               = 1;
    return 1;
}

const ahci_info_t* ahci_info(void) { return &info; }

uint32_t ahci_active_ports(void) {
    if (!info.present) return 0;
    uint32_t active = 0;
    for (uint32_t i = 0; i < info.num_ports; i++) {
        if (!(info.port_implemented_mask & (1u << i))) continue;
        uint32_t ssts = mmio_r32(port_base(i) + PORT_SSTS);
        uint8_t det = ssts & 0xF;
        uint8_t ipm = (ssts >> 8) & 0xF;
        if (det == 3 && ipm == 1) active |= (1u << i);
    }
    return active;
}

// ---- vC.6.1: per-port command list bring-up ----

static int port_stop(uint32_t port) {
    uint64_t pb = port_base(port);
    uint32_t cmd = mmio_r32(pb + PORT_CMD);
    cmd &= ~(PXCMD_ST | PXCMD_FRE);
    mmio_w32(pb + PORT_CMD, cmd);
    // Wait for CR + FR to clear (bounded spin)
    for (int t = 0; t < 1000000; t++) {
        uint32_t s = mmio_r32(pb + PORT_CMD);
        if (!(s & (PXCMD_CR | PXCMD_FR))) return 1;
    }
    return 0;
}

static void port_start(uint32_t port) {
    uint64_t pb = port_base(port);
    // Spin until BSY clears
    for (int t = 0; t < 1000000; t++) {
        if (!(mmio_r32(pb + PORT_TFD) & PXTFD_BSY)) break;
    }
    uint32_t cmd = mmio_r32(pb + PORT_CMD);
    mmio_w32(pb + PORT_CMD, cmd | PXCMD_FRE);
    mmio_w32(pb + PORT_CMD, cmd | PXCMD_FRE | PXCMD_ST);
}

int ahci_port_init(uint32_t port) {
    if (!info.present) return 0;
    if (!(ahci_active_ports() & (1u << port))) return 0;
    if (!port_stop(port)) return 0;

    uint64_t pb = port_base(port);

    // Zero the structures
    for (uint32_t i = 0; i < sizeof(g_clb); i++) ((uint8_t*)g_clb)[i] = 0;
    for (uint32_t i = 0; i < sizeof(g_fb);  i++) g_fb[i] = 0;
    for (uint32_t i = 0; i < sizeof(g_cmdtbl); i++) ((uint8_t*)&g_cmdtbl)[i] = 0;

    // Wire slot 0's header to our command table
    g_clb[0].ctba  = (uint32_t)(uintptr_t)&g_cmdtbl;
    g_clb[0].ctbau = 0;

    // Program CLB and FB on the port (low-mem assumption; ctbau==0 ok)
    mmio_w32(pb + PORT_CLB,  (uint32_t)(uintptr_t)g_clb);
    mmio_w32(pb + PORT_CLBU, 0);
    mmio_w32(pb + PORT_FB,   (uint32_t)(uintptr_t)g_fb);
    mmio_w32(pb + PORT_FBU,  0);

    // Clear pending IS bits, then start FIS RX + cmd engine
    mmio_w32(pb + PORT_SERR, 0xFFFFFFFFu);
    mmio_w32(pb + PORT_IS,   0xFFFFFFFFu);
    port_start(port);
    return 1;
}

int ahci_port_read(uint32_t port, uint64_t lba, uint32_t count, void *buf) {
    if (!info.present || !buf || count == 0 || count > 8) return 0;

    uint64_t pb = port_base(port);

    // Build the command header (slot 0)
    g_clb[0].flags = (uint16_t)((sizeof(uint32_t) * 5) >> 1);  // CFL = 5 dwords for H2D
    g_clb[0].prdtl = 1;
    g_clb[0].prdbc = 0;

    // Build the PRD entry
    hba_prdt_entry_t *prd = &g_cmdtbl.prdt[0];
    prd->dba  = (uint32_t)(uintptr_t)buf;
    prd->dbau = 0;
    prd->rsv0 = 0;
    prd->dbc  = (count * 512u) - 1u;   // byte count - 1; bit 0 set means odd

    // Build the H2D register FIS: ATA READ_DMA_EX (0x25), LBA48
    uint8_t *fis = g_cmdtbl.cfis;
    for (int i = 0; i < 64; i++) fis[i] = 0;
    fis[0] = FIS_TYPE_REG_H2D;
    fis[1] = (1u << 7);                // C=1: command, not control
    fis[2] = 0x25;                     // READ_DMA_EX
    fis[3] = 0;                        // features (low)

    fis[4] = (uint8_t)(lba >>  0);
    fis[5] = (uint8_t)(lba >>  8);
    fis[6] = (uint8_t)(lba >> 16);
    fis[7] = (1u << 6);                // device: LBA mode

    fis[8] = (uint8_t)(lba >> 24);
    fis[9] = (uint8_t)(lba >> 32);
    fis[10]= (uint8_t)(lba >> 40);
    fis[11]= 0;                        // features (high)

    fis[12]= (uint8_t)(count & 0xFF);
    fis[13]= (uint8_t)((count >> 8) & 0xFF);

    // Wait until not BSY/DRQ
    for (int t = 0; t < 1000000; t++) {
        if (!(mmio_r32(pb + PORT_TFD) & (PXTFD_BSY | PXTFD_DRQ))) break;
    }

    // Issue slot 0
    mmio_w32(pb + PORT_CI, 1u);

    // Wait for completion
    for (int t = 0; t < 5000000; t++) {
        if (!(mmio_r32(pb + PORT_CI) & 1u)) {
            // Check for task-file error
            if (mmio_r32(pb + PORT_TFD) & 0x01) return 0;
            return 1;
        }
    }
    return 0;
}
