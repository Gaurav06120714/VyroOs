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

// vC.6.4: per-port static allocations. AHCI tops out at 32 ports per spec,
// but in practice 1-6 are present on real hardware. Keep BSS budget sane
// by giving each port a smaller 8-slot command list (256 bytes/port) and
// one command table per port.
#define AHCI_MAX_PORTS  32
#define AHCI_PORT_SLOTS 8

static __attribute__((aligned(1024))) hba_cmd_header_t g_clb_pool[AHCI_MAX_PORTS][AHCI_PORT_SLOTS];
static __attribute__((aligned(256)))  uint8_t           g_fb_pool[AHCI_MAX_PORTS][256];
static __attribute__((aligned(128)))  hba_cmd_table_t   g_cmdtbl_pool[AHCI_MAX_PORTS];

/* Per-port bring-up status bitmap. */
static uint32_t g_port_ready_mask = 0;

/* Compatibility aliases for legacy single-port code paths. */
#define g_clb    (g_clb_pool[0])
#define g_fb     (g_fb_pool[0])
#define g_cmdtbl (g_cmdtbl_pool[0])

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
    if (port >= AHCI_MAX_PORTS) return 0;
    if (!(ahci_active_ports() & (1u << port))) return 0;
    if (!port_stop(port)) return 0;

    uint64_t pb = port_base(port);
    hba_cmd_header_t *clb    = g_clb_pool[port];
    uint8_t          *fb     = g_fb_pool[port];
    hba_cmd_table_t  *cmdtbl = &g_cmdtbl_pool[port];

    /* Zero this port's structures only — siblings keep theirs intact */
    for (uint32_t i = 0; i < sizeof(g_clb_pool[port]);    i++) ((uint8_t*)clb)[i]    = 0;
    for (uint32_t i = 0; i < sizeof(g_fb_pool[port]);     i++) fb[i]                  = 0;
    for (uint32_t i = 0; i < sizeof(g_cmdtbl_pool[port]); i++) ((uint8_t*)cmdtbl)[i] = 0;

    /* Wire slot 0's header to this port's command table */
    clb[0].ctba  = (uint32_t)(uintptr_t)cmdtbl;
    clb[0].ctbau = 0;

    mmio_w32(pb + PORT_CLB,  (uint32_t)(uintptr_t)clb);
    mmio_w32(pb + PORT_CLBU, 0);
    mmio_w32(pb + PORT_FB,   (uint32_t)(uintptr_t)fb);
    mmio_w32(pb + PORT_FBU,  0);

    mmio_w32(pb + PORT_SERR, 0xFFFFFFFFu);
    mmio_w32(pb + PORT_IS,   0xFFFFFFFFu);
    port_start(port);

    g_port_ready_mask |= (1u << port);
    return 1;
}

int ahci_port_read(uint32_t port, uint64_t lba, uint32_t count, void *buf) {
    if (!info.present || !buf || count == 0 || count > 8) return 0;
    if (port >= AHCI_MAX_PORTS) return 0;
    if (!(g_port_ready_mask & (1u << port))) return 0;

    uint64_t pb = port_base(port);
    hba_cmd_header_t *clb    = g_clb_pool[port];
    hba_cmd_table_t  *cmdtbl = &g_cmdtbl_pool[port];

    // Build the command header (slot 0)
    clb[0].flags = (uint16_t)((sizeof(uint32_t) * 5) >> 1);  // CFL = 5 dwords for H2D
    clb[0].prdtl = 1;
    clb[0].prdbc = 0;

    // Build the PRD entry
    hba_prdt_entry_t *prd = &cmdtbl->prdt[0];
    prd->dba  = (uint32_t)(uintptr_t)buf;
    prd->dbau = 0;
    prd->rsv0 = 0;
    prd->dbc  = (count * 512u) - 1u;   // byte count - 1; bit 0 set means odd

    // Build the H2D register FIS: ATA READ_DMA_EX (0x25), LBA48
    uint8_t *fis = cmdtbl->cfis;
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

// =================================================================
// vC.6.7 — ATA WRITE_DMA_EX + IDENTIFY
// =================================================================

#define PXCMD_W_BIT   (1u << 6)   // command-header flags: write direction

// Shared issue path for read/write: builds the same H2D FIS with a
// caller-supplied ATA opcode and direction flag, points the single PRD
// at the supplied buffer, kicks slot 0, waits for CI to clear.
static int port_rw(uint32_t port, uint8_t ata_op, int is_write,
                   uint64_t lba, uint32_t count, void *buf) {
    if (!info.present || !buf || count == 0 || count > 8) return 0;
    if (port >= AHCI_MAX_PORTS) return 0;
    if (!(g_port_ready_mask & (1u << port))) return 0;

    uint64_t pb = port_base(port);
    hba_cmd_header_t *clb    = g_clb_pool[port];
    hba_cmd_table_t  *cmdtbl = &g_cmdtbl_pool[port];

    uint16_t flags = (uint16_t)((sizeof(uint32_t) * 5) >> 1);     // CFL=5
    if (is_write) flags |= PXCMD_W_BIT;
    clb[0].flags = flags;
    clb[0].prdtl = 1;
    clb[0].prdbc = 0;

    hba_prdt_entry_t *prd = &cmdtbl->prdt[0];
    prd->dba  = (uint32_t)(uintptr_t)buf;
    prd->dbau = 0;
    prd->rsv0 = 0;
    prd->dbc  = (count * 512u) - 1u;

    uint8_t *fis = cmdtbl->cfis;
    for (int i = 0; i < 64; i++) fis[i] = 0;
    fis[0] = FIS_TYPE_REG_H2D;
    fis[1] = (1u << 7);
    fis[2] = ata_op;
    fis[3] = 0;

    fis[4] = (uint8_t)(lba >>  0);
    fis[5] = (uint8_t)(lba >>  8);
    fis[6] = (uint8_t)(lba >> 16);
    fis[7] = (1u << 6);    // LBA mode

    fis[8] = (uint8_t)(lba >> 24);
    fis[9] = (uint8_t)(lba >> 32);
    fis[10]= (uint8_t)(lba >> 40);
    fis[11]= 0;

    fis[12]= (uint8_t)(count & 0xFF);
    fis[13]= (uint8_t)((count >> 8) & 0xFF);

    for (int t = 0; t < 1000000; t++) {
        if (!(mmio_r32(pb + PORT_TFD) & (PXTFD_BSY | PXTFD_DRQ))) break;
    }
    mmio_w32(pb + PORT_CI, 1u);

    for (int t = 0; t < 5000000; t++) {
        if (!(mmio_r32(pb + PORT_CI) & 1u)) {
            if (mmio_r32(pb + PORT_TFD) & 0x01) return 0;
            return 1;
        }
    }
    return 0;
}

int ahci_port_write(uint32_t port, uint64_t lba, uint32_t count, const void *buf) {
    return port_rw(port, /*ata_op=WRITE_DMA_EX*/0x35, /*is_write=*/1,
                   lba, count, (void *)buf);
}

// Strip trailing spaces, NUL-terminate.
static void rtrim_pad(char *s) {
    int n = 0; while (s[n]) n++;
    while (n > 0 && s[n-1] == ' ') n--;
    s[n] = '\0';
}

int ahci_port_identify(uint32_t port, char *out_model, char *out_serial,
                       uint64_t *out_total_sectors) {
    if (!info.present || port >= AHCI_MAX_PORTS) return 0;
    if (!(g_port_ready_mask & (1u << port))) return 0;

    // 512-byte response per ATA spec. Use the per-port command table's
    // dba pointer area by allocating a stack-aligned buffer here.
    static __attribute__((aligned(4))) uint8_t ident[512];
    for (uint32_t i = 0; i < sizeof(ident); i++) ident[i] = 0;

    uint64_t pb = port_base(port);
    hba_cmd_header_t *clb    = g_clb_pool[port];
    hba_cmd_table_t  *cmdtbl = &g_cmdtbl_pool[port];

    clb[0].flags = (uint16_t)((sizeof(uint32_t) * 5) >> 1);
    clb[0].prdtl = 1;
    clb[0].prdbc = 0;

    hba_prdt_entry_t *prd = &cmdtbl->prdt[0];
    prd->dba = (uint32_t)(uintptr_t)ident; prd->dbau = 0; prd->rsv0 = 0;
    prd->dbc = 512u - 1u;

    uint8_t *fis = cmdtbl->cfis;
    for (int i = 0; i < 64; i++) fis[i] = 0;
    fis[0] = FIS_TYPE_REG_H2D;
    fis[1] = (1u << 7);
    fis[2] = 0xEC;  // ATA IDENTIFY DEVICE
    /* No LBA, no count for IDENTIFY */

    for (int t = 0; t < 1000000; t++) {
        if (!(mmio_r32(pb + PORT_TFD) & (PXTFD_BSY | PXTFD_DRQ))) break;
    }
    mmio_w32(pb + PORT_CI, 1u);
    int ok = 0;
    for (int t = 0; t < 5000000; t++) {
        if (!(mmio_r32(pb + PORT_CI) & 1u)) {
            ok = !(mmio_r32(pb + PORT_TFD) & 0x01);
            break;
        }
    }
    if (!ok) return 0;

    // Words are little-endian on the wire, but ASCII strings are stored
    // big-endian *within* each word (legacy ATA "high byte first per word").
    // Words 10..19 = serial (20 bytes), words 27..46 = model (40 bytes).
    if (out_serial) {
        for (int w = 0; w < 10; w++) {
            uint16_t v = ident[(10 + w)*2] | (ident[(10 + w)*2 + 1] << 8);
            out_serial[w*2 + 0] = (char)((v >> 8) & 0xFF);
            out_serial[w*2 + 1] = (char)(v & 0xFF);
        }
        out_serial[20] = '\0';
        rtrim_pad(out_serial);
    }
    if (out_model) {
        for (int w = 0; w < 20; w++) {
            uint16_t v = ident[(27 + w)*2] | (ident[(27 + w)*2 + 1] << 8);
            out_model[w*2 + 0] = (char)((v >> 8) & 0xFF);
            out_model[w*2 + 1] = (char)(v & 0xFF);
        }
        out_model[40] = '\0';
        rtrim_pad(out_model);
    }
    if (out_total_sectors) {
        // Words 100..103 = user-addressable LBA48 max sectors (64-bit)
        uint64_t lba48 = 0;
        for (int w = 0; w < 4; w++) {
            uint64_t v = ident[(100 + w)*2] | (ident[(100 + w)*2 + 1] << 8);
            lba48 |= v << (w * 16);
        }
        *out_total_sectors = lba48;
    }
    return 1;
}
