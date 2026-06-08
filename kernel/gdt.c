#include "gdt.h"

typedef struct {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist[7];
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
} __attribute__((packed)) tss_t;

typedef struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed)) gdt_entry_t;

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

static uint64_t gdt[7];
static tss_t    tss;
static gdt_ptr_t gdt_ptr;

static uint8_t kernel_stack[16384] __attribute__((aligned(16)));

extern void gdt_flush(uint64_t gdt_ptr_addr);
extern void tss_flush(uint16_t selector);

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

void tss_set_rsp0(uint64_t rsp0) {
    tss.rsp0 = rsp0;
}

void gdt_init() {




    gdt[0] = 0;
    gdt[1] = make_segment(0x9A, 0xA0);
    gdt[2] = make_segment(0x92, 0xC0);
    gdt[3] = make_segment(0xFA, 0xA0);
    gdt[4] = make_segment(0xF2, 0xC0);


    for (uint32_t i = 0; i < sizeof(tss); i++) ((uint8_t*)&tss)[i] = 0;
    tss.rsp0       = (uint64_t)(kernel_stack + sizeof(kernel_stack));
    tss.iomap_base = sizeof(tss);

    uint64_t base  = (uint64_t)&tss;
    uint32_t limit = sizeof(tss) - 1;

    tss_descriptor_t td;
    td.limit_low   = (uint16_t)(limit & 0xFFFF);
    td.base_low    = (uint16_t)(base & 0xFFFF);
    td.base_mid    = (uint8_t)((base >> 16) & 0xFF);
    td.access      = 0x89;
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
