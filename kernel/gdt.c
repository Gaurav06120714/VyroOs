#include "gdt.h"

// ─────────────────────────────────────────────────
// 64-bit TSS (Task State Segment) — 104 bytes.
// We only use rsp0: the kernel stack the CPU loads
// when an interrupt happens while running in ring 3.
// ─────────────────────────────────────────────────
typedef struct {
    uint32_t reserved0;
    uint64_t rsp0;        // Ring 0 stack pointer
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist[7];
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
} __attribute__((packed)) tss_t;

// ─────────────────────────────────────────────────
// GDT entry (8 bytes) — used for code/data segments
// ─────────────────────────────────────────────────
typedef struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed)) gdt_entry_t;

// TSS descriptor in long mode is 16 bytes (two GDT slots)
typedef struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
    uint32_t base_upper;
    uint32_t reserved;
} __attribute__((packed)) tss_descriptor_t;

typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) gdt_ptr_t;

// ─────────────────────────────────────────────────
// GDT storage: null, kcode, kdata, ucode, udata, TSS(2 slots)
// 7 quadwords total = 56 bytes
// ─────────────────────────────────────────────────
static uint64_t gdt[7];
static tss_t    tss;
static gdt_ptr_t gdt_ptr;

// Kernel stack used when entering interrupts from ring 3
static uint8_t kernel_stack[16384] __attribute__((aligned(16)));

// Defined in gdt_flush.asm
extern void gdt_flush(uint64_t gdt_ptr_addr);
extern void tss_flush(uint16_t selector);

// ─────────────────────────────────────────────────
// Build a standard code/data segment descriptor
// ─────────────────────────────────────────────────
static uint64_t make_segment(uint8_t access, uint8_t flags) {
    gdt_entry_t e;
    e.limit_low   = 0xFFFF;
    e.base_low    = 0;
    e.base_mid    = 0;
    e.access      = access;
    e.granularity = (uint8_t)(0x0F | (flags & 0xF0));
    e.base_high   = 0;
    uint64_t v;
    __builtin_memcpy(&v, &e, sizeof(v));
    return v;
}

// ─────────────────────────────────────────────────
// tss_set_rsp0: update the ring-0 stack the CPU uses
// when transitioning from ring 3 to ring 0
// ─────────────────────────────────────────────────
void tss_set_rsp0(uint64_t rsp0) {
    tss.rsp0 = rsp0;
}

// ─────────────────────────────────────────────────
// gdt_init: build the GDT + TSS and load them
// ─────────────────────────────────────────────────
void gdt_init() {
    // Access byte: P=1, DPL, S=1(code/data), then type bits
    // Code: 0x9A (ring0) / 0xFA (ring3). Data: 0x92 / 0xF2.
    // Flags: granularity byte high nibble. 0xA0 = long mode (L=1) for code.

    gdt[0] = 0;                                   // null
    gdt[1] = make_segment(0x9A, 0xA0);            // 0x08 kernel code (L=1)
    gdt[2] = make_segment(0x92, 0xC0);            // 0x10 kernel data
    gdt[3] = make_segment(0xFA, 0xA0);            // 0x18 user code (DPL3, L=1)
    gdt[4] = make_segment(0xF2, 0xC0);            // 0x20 user data (DPL3)

    // TSS descriptor occupies gdt[5] and gdt[6] (16 bytes)
    for (uint32_t i = 0; i < sizeof(tss); i++) ((uint8_t*)&tss)[i] = 0;
    tss.rsp0       = (uint64_t)(kernel_stack + sizeof(kernel_stack));
    tss.iomap_base = sizeof(tss);

    uint64_t base  = (uint64_t)&tss;
    uint32_t limit = sizeof(tss) - 1;

    tss_descriptor_t td;
    td.limit_low   = (uint16_t)(limit & 0xFFFF);
    td.base_low    = (uint16_t)(base & 0xFFFF);
    td.base_mid    = (uint8_t)((base >> 16) & 0xFF);
    td.access      = 0x89;                        // present, type=available 64-bit TSS
    td.granularity = (uint8_t)((limit >> 16) & 0x0F);
    td.base_high   = (uint8_t)((base >> 24) & 0xFF);
    td.base_upper  = (uint32_t)(base >> 32);
    td.reserved    = 0;
    __builtin_memcpy(&gdt[5], &td, sizeof(td));

    gdt_ptr.limit = sizeof(gdt) - 1;
    gdt_ptr.base  = (uint64_t)&gdt;

    gdt_flush((uint64_t)&gdt_ptr);
    tss_flush(SEL_TSS);
}
