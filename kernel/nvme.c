#include "nvme.h"
#include "pci.h"

static nvme_info_t info;

// --- NVMe MMIO register offsets ---
#define NVME_CAP    0x00    // 64-bit
#define NVME_VS     0x08
#define NVME_CC     0x14
#define NVME_CSTS   0x1C
#define NVME_AQA    0x24
#define NVME_ASQ    0x28    // 64-bit
#define NVME_ACQ    0x30    // 64-bit
#define NVME_SQ0TDBL 0x1000 // doorbell base; stride from CAP.DSTRD

#define CC_EN       (1u << 0)
#define CC_IOSQES_6 (6u << 16)   // 64 bytes (1 << 6)
#define CC_IOCQES_4 (4u << 20)   // 16 bytes
#define CSTS_RDY    (1u << 0)

// --- Submission Queue Entry (64 bytes) ---
typedef struct __attribute__((packed)) {
    uint8_t  opcode;
    uint8_t  flags;
    uint16_t cid;
    uint32_t nsid;
    uint64_t rsv0;
    uint64_t mptr;
    uint64_t prp1;
    uint64_t prp2;
    uint32_t cdw10;
    uint32_t cdw11;
    uint32_t cdw12;
    uint32_t cdw13;
    uint32_t cdw14;
    uint32_t cdw15;
} nvme_sqe_t;

// --- Completion Queue Entry (16 bytes) ---
typedef struct __attribute__((packed)) {
    uint32_t cdw0;
    uint32_t cdw1;
    uint16_t sq_head;
    uint16_t sq_id;
    uint16_t cid;
    uint16_t status;        // phase bit in bit 0
} nvme_cqe_t;

// Admin queue depth: 8 entries each (256 bytes ASQ, 128 bytes ACQ)
#define ASQ_DEPTH 8
#define ACQ_DEPTH 8

// Static allocations for the admin queues + one I/O buffer (4 KiB).
// Page-aligned per NVMe spec.
static __attribute__((aligned(4096))) nvme_sqe_t g_asq[ASQ_DEPTH];
static __attribute__((aligned(4096))) nvme_cqe_t g_acq[ACQ_DEPTH];
static __attribute__((aligned(4096))) uint8_t    g_ident_buf[4096];

static uint16_t g_sq_tail = 0;
static uint16_t g_cq_head = 0;
static uint8_t  g_cq_phase = 1;

static inline uint64_t mmio_r64(uint64_t addr) {
    uint64_t lo = *(volatile uint32_t*)addr;
    uint64_t hi = *(volatile uint32_t*)(addr + 4);
    return (hi << 32) | lo;
}
static inline uint32_t mmio_r32(uint64_t addr) { return *(volatile uint32_t*)addr; }
static inline void     mmio_w32(uint64_t addr, uint32_t v) { *(volatile uint32_t*)addr = v; }
static inline void     mmio_w64(uint64_t addr, uint64_t v) {
    *(volatile uint32_t*)addr       = (uint32_t)v;
    *(volatile uint32_t*)(addr + 4) = (uint32_t)(v >> 32);
}

int nvme_init(void) {
    for (uint32_t i = 0; i < sizeof(info); i++) ((uint8_t*)&info)[i] = 0;

    pci_device_t* dev = pci_find_class(0x01, 0x08);
    if (!dev || dev->prog_if != 0x02) return 0;

    extern uint32_t pci_config_read(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off);
    uint32_t bar0_lo = pci_config_read(dev->bus, dev->slot, dev->func, 0x10);
    uint32_t bar0_hi = pci_config_read(dev->bus, dev->slot, dev->func, 0x14);
    uint64_t base;
    if ((bar0_lo & 0x6) == 0x4) base = ((uint64_t)bar0_hi << 32) | (bar0_lo & ~0xFu);
    else                        base = bar0_lo & ~0xFu;
    if (!base) return 0;

    info.mmio_base       = base;
    info.cap             = mmio_r64(base + NVME_CAP);
    info.version         = mmio_r32(base + NVME_VS);
    info.max_q_entries   = (info.cap & 0xFFFF) + 1;
    info.doorbell_stride = (info.cap >> 32) & 0xF;
    info.present         = 1;
    return 1;
}

const nvme_info_t* nvme_info(void) { return &info; }

// ---- vC.6.2: Admin Queue bring-up + Identify ----

static inline uint64_t sq_tail_dbl(void) {
    // SQ0 tail doorbell at offset 0x1000; stride is 4 << DSTRD.
    return info.mmio_base + NVME_SQ0TDBL + (0 * (4u << info.doorbell_stride));
}
static inline uint64_t cq_head_dbl(void) {
    // CQ0 head doorbell is at SQ0TDBL + (1 * (4 << DSTRD))
    return info.mmio_base + NVME_SQ0TDBL + (1u * (4u << info.doorbell_stride));
}

static int admin_submit(nvme_sqe_t *sqe) {
    sqe->cid = (uint16_t)g_sq_tail;
    g_asq[g_sq_tail] = *sqe;

    g_sq_tail = (uint16_t)((g_sq_tail + 1) % ASQ_DEPTH);
    mmio_w32(sq_tail_dbl(), g_sq_tail);

    // Spin for completion (no IRQs in vC.6.2)
    for (int t = 0; t < 50000000; t++) {
        volatile nvme_cqe_t *c = &g_acq[g_cq_head];
        if ((c->status & 1) == g_cq_phase) {
            uint16_t sf = (c->status >> 1) & 0x7FFF;
            g_cq_head = (uint16_t)((g_cq_head + 1) % ACQ_DEPTH);
            if (g_cq_head == 0) g_cq_phase ^= 1;
            mmio_w32(cq_head_dbl(), g_cq_head);
            return sf == 0 ? 1 : 0;
        }
    }
    return 0;
}

static void copy_ascii(char *dst, const uint8_t *src, int len) {
    int j = 0;
    for (int i = 0; i < len; i++) {
        char c = (char)src[i];
        if (c >= 0x20 && c <= 0x7E) dst[j++] = c;
    }
    while (j > 0 && dst[j - 1] == ' ') j--;
    dst[j] = '\0';
}

int nvme_admin_init(void) {
    if (!info.present) return 0;

    // 1. Disable controller (CC.EN = 0) and wait for CSTS.RDY = 0
    uint32_t cc = mmio_r32(info.mmio_base + NVME_CC);
    mmio_w32(info.mmio_base + NVME_CC, cc & ~CC_EN);
    for (int t = 0; t < 1000000; t++) {
        if (!(mmio_r32(info.mmio_base + NVME_CSTS) & CSTS_RDY)) break;
    }

    // 2. Program AQA: ACQS in [27:16], ASQS in [11:0], both = depth-1
    uint32_t aqa = ((ACQ_DEPTH - 1) << 16) | (ASQ_DEPTH - 1);
    mmio_w32(info.mmio_base + NVME_AQA, aqa);
    mmio_w64(info.mmio_base + NVME_ASQ, (uint64_t)(uintptr_t)g_asq);
    mmio_w64(info.mmio_base + NVME_ACQ, (uint64_t)(uintptr_t)g_acq);

    // 3. Zero the queues + reset our shadow indices
    for (uint32_t i = 0; i < sizeof(g_asq); i++) ((uint8_t*)g_asq)[i] = 0;
    for (uint32_t i = 0; i < sizeof(g_acq); i++) ((uint8_t*)g_acq)[i] = 0;
    g_sq_tail = 0; g_cq_head = 0; g_cq_phase = 1;

    // 4. Enable controller: IOSQES=6, IOCQES=4, EN=1
    mmio_w32(info.mmio_base + NVME_CC, CC_IOSQES_6 | CC_IOCQES_4 | CC_EN);

    // 5. Wait for CSTS.RDY = 1
    int ready = 0;
    for (int t = 0; t < 5000000; t++) {
        if (mmio_r32(info.mmio_base + NVME_CSTS) & CSTS_RDY) { ready = 1; break; }
    }
    if (!ready) return 0;

    // 6. Identify Controller (opcode 0x06, CNS=1)
    nvme_sqe_t sqe = {0};
    sqe.opcode = 0x06;
    sqe.nsid   = 0;
    sqe.prp1   = (uint64_t)(uintptr_t)g_ident_buf;
    sqe.cdw10  = 1;   // CNS = 1: Identify Controller
    for (uint32_t i = 0; i < sizeof(g_ident_buf); i++) g_ident_buf[i] = 0;
    if (!admin_submit(&sqe)) return 0;

    // Bytes 4..23 = Serial Number (ASCII), 24..63 = Model Number
    copy_ascii(info.serial, g_ident_buf + 4,  20);
    copy_ascii(info.model,  g_ident_buf + 24, 40);

    // 7. Identify Namespace 1 (CNS = 0, NSID = 1)
    for (uint32_t i = 0; i < sizeof(sqe);          i++) ((uint8_t*)&sqe)[i] = 0;
    for (uint32_t i = 0; i < sizeof(g_ident_buf);  i++) g_ident_buf[i] = 0;
    sqe.opcode = 0x06;
    sqe.nsid   = 1;
    sqe.prp1   = (uint64_t)(uintptr_t)g_ident_buf;
    sqe.cdw10  = 0;
    if (!admin_submit(&sqe)) return 0;

    // NSZE @ bytes 0..7 (LBAs), FLBAS @ byte 26 (low 4 bits = LBA fmt index)
    uint64_t nsze = 0;
    for (int i = 7; i >= 0; i--) { nsze = (nsze << 8) | g_ident_buf[i]; }
    info.ns1_size_blocks = nsze;

    uint8_t flbas = g_ident_buf[26] & 0xF;
    // LBA Format Data @ byte 128 + 4*idx, LBADS in bits [23:16] of dword
    uint32_t lbaf = 0;
    for (int i = 3; i >= 0; i--) lbaf = (lbaf << 8) | g_ident_buf[128 + 4 * flbas + i];
    uint32_t lbads = (lbaf >> 16) & 0xFF;
    info.ns1_lba_bytes = (lbads >= 9 && lbads <= 16) ? (1u << lbads) : 512u;

    info.admin_ready = 1;
    return 1;
}

// =================================================================
// vC.6.3 — I/O Queue 1 + READ/WRITE
// =================================================================

#define IOQ_DEPTH 8

static __attribute__((aligned(4096))) nvme_sqe_t g_iosq[IOQ_DEPTH];
static __attribute__((aligned(4096))) nvme_cqe_t g_iocq[IOQ_DEPTH];

static uint16_t g_iosq_tail = 0;
static uint16_t g_iocq_head = 0;
static uint8_t  g_iocq_phase = 1;

// I/O queue doorbells live at SQ0TDBL + (2*qid) * (4 << DSTRD) for SQs,
// and the matching CQ head doorbell at +1 from the SQ doorbell.
static inline uint64_t iosq_tail_dbl(void) {
    return info.mmio_base + NVME_SQ0TDBL + (2u * 1u) * (4u << info.doorbell_stride);
}
static inline uint64_t iocq_head_dbl(void) {
    return info.mmio_base + NVME_SQ0TDBL + (2u * 1u + 1u) * (4u << info.doorbell_stride);
}

static int io_submit(nvme_sqe_t *sqe) {
    sqe->cid = (uint16_t)g_iosq_tail;
    g_iosq[g_iosq_tail] = *sqe;
    g_iosq_tail = (uint16_t)((g_iosq_tail + 1) % IOQ_DEPTH);
    mmio_w32(iosq_tail_dbl(), g_iosq_tail);

    for (int t = 0; t < 50000000; t++) {
        volatile nvme_cqe_t *c = &g_iocq[g_iocq_head];
        if ((c->status & 1) == g_iocq_phase) {
            uint16_t sf = (c->status >> 1) & 0x7FFF;
            g_iocq_head = (uint16_t)((g_iocq_head + 1) % IOQ_DEPTH);
            if (g_iocq_head == 0) g_iocq_phase ^= 1;
            mmio_w32(iocq_head_dbl(), g_iocq_head);
            return sf == 0 ? 1 : 0;
        }
    }
    return 0;
}

int nvme_io_init(void) {
    if (!info.admin_ready) return 0;

    for (uint32_t i = 0; i < sizeof(g_iosq); i++) ((uint8_t*)g_iosq)[i] = 0;
    for (uint32_t i = 0; i < sizeof(g_iocq); i++) ((uint8_t*)g_iocq)[i] = 0;
    g_iosq_tail = 0; g_iocq_head = 0; g_iocq_phase = 1;

    // 1. Create I/O Completion Queue 1 — admin opcode 0x05
    nvme_sqe_t sqe = {0};
    sqe.opcode = 0x05;
    sqe.prp1   = (uint64_t)(uintptr_t)g_iocq;
    // cdw10: QSIZE (upper 16, depth-1) | QID (lower 16)
    sqe.cdw10  = ((IOQ_DEPTH - 1u) << 16) | 1u;
    // cdw11: bit 0 = Physically Contiguous, bit 1 = Interrupts Enabled (off here)
    sqe.cdw11  = 1u;
    if (!admin_submit(&sqe)) return 0;

    // 2. Create I/O Submission Queue 1 — admin opcode 0x01, bound to CQ 1
    for (uint32_t i = 0; i < sizeof(sqe); i++) ((uint8_t*)&sqe)[i] = 0;
    sqe.opcode = 0x01;
    sqe.prp1   = (uint64_t)(uintptr_t)g_iosq;
    sqe.cdw10  = ((IOQ_DEPTH - 1u) << 16) | 1u;
    // cdw11: bit 0 = PC, bits 31:16 = CQID
    sqe.cdw11  = 1u | (1u << 16);
    if (!admin_submit(&sqe)) return 0;
    return 1;
}

static int nvme_rw(uint8_t opcode, uint64_t lba, uint32_t count, void *buf) {
    if (!info.admin_ready) return 0;
    if (!buf || count == 0) return 0;
    // Single-PRP path: total bytes must fit in one page (4 KiB after the
    // page-aligned buffer); larger transfers land in vC.6.4 alongside PRP lists.
    uint32_t bytes = count * info.ns1_lba_bytes;
    if (bytes > 4096) return 0;

    nvme_sqe_t sqe = {0};
    sqe.opcode = opcode;
    sqe.nsid   = 1;
    sqe.prp1   = (uint64_t)(uintptr_t)buf;
    sqe.cdw10  = (uint32_t)(lba & 0xFFFFFFFFu);
    sqe.cdw11  = (uint32_t)(lba >> 32);
    // cdw12: NLB (number of LBAs minus 1) in low 16 bits
    sqe.cdw12  = (count - 1u) & 0xFFFFu;
    return io_submit(&sqe);
}

int nvme_read (uint64_t lba, uint32_t count, void       *buf) { return nvme_rw(0x02, lba, count, buf); }
int nvme_write(uint64_t lba, uint32_t count, const void *buf) { return nvme_rw(0x01, lba, count, (void *)buf); }
