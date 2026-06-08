#include "lapic.h"

static uint64_t base = 0;
static int      enabled = 0;

static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t lo, hi;
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}
static inline void wrmsr(uint32_t msr, uint64_t v) {
    __asm__ volatile("wrmsr" : : "a"((uint32_t)v), "d"((uint32_t)(v >> 32)), "c"(msr));
}
static inline uint32_t mmio_r32(uint64_t addr) {
    return *(volatile uint32_t*)addr;
}
static inline void mmio_w32(uint64_t addr, uint32_t v) {
    *(volatile uint32_t*)addr = v;
}

int lapic_init(void) {

    uint64_t apic_base_msr = rdmsr(0x1B);
    base = apic_base_msr & 0xFFFFFFFFFF000ULL;
    if (base == 0) base = LAPIC_DEFAULT_BASE;


    wrmsr(0x1B, apic_base_msr | (1ULL << 11));


    uint32_t svr = mmio_r32(base + LAPIC_REG_SVR);
    mmio_w32(base + LAPIC_REG_SVR, (svr & ~0xFFu) | 0xFF | (1u << 8));


    mmio_w32(base + LAPIC_REG_TPR, 0);

    enabled = 1;
    return 1;
}

int      lapic_is_enabled(void) { return enabled; }
uint64_t lapic_base(void)       { return base; }

uint32_t lapic_id(void) {
    if (!enabled) return 0;
    return mmio_r32(base + LAPIC_REG_ID) >> 24;
}
uint32_t lapic_version(void) {
    if (!enabled) return 0;
    return mmio_r32(base + LAPIC_REG_VERSION);
}

void lapic_send_ipi(uint32_t dest_apic_id, uint32_t icr_lo) {
    if (!enabled) return;
    mmio_w32(base + LAPIC_REG_ICR_HI, dest_apic_id << 24);
    mmio_w32(base + LAPIC_REG_ICR_LO, icr_lo);

    for (int i = 0; i < 1000000; i++) {
        if (!(mmio_r32(base + LAPIC_REG_ICR_LO) & (1u << 12))) break;
    }
}

void lapic_eoi(void) {
    if (!enabled) return;
    mmio_w32(base + LAPIC_REG_EOI, 0);
}
