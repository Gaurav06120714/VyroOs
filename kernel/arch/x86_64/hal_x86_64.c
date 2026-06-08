#include "../hal.h"

const char* hal_arch_name(void) { return "x86_64"; }

static inline uint64_t rdtsc(void) {
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}
uint64_t hal_cycle_counter(void) { return rdtsc(); }

static void cpuid(uint32_t leaf, uint32_t sub,
                  uint32_t* a, uint32_t* b, uint32_t* c, uint32_t* d) {
    __asm__ volatile("cpuid" : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d)
                              : "a"(leaf), "c"(sub));
}

void hal_cpu_detect(hal_cpu_info_t* info) {
    for (uint32_t i = 0; i < sizeof(*info); i++) ((uint8_t*)info)[i] = 0;
    uint32_t a, b, c, d;


    cpuid(0, 0, &a, &b, &c, &d);
    uint32_t max_basic = a;
    for (int i = 0; i < 4; i++) info->vendor[0 + i] = (char)(b >> (i * 8));
    for (int i = 0; i < 4; i++) info->vendor[4 + i] = (char)(d >> (i * 8));
    for (int i = 0; i < 4; i++) info->vendor[8 + i] = (char)(c >> (i * 8));
    info->vendor[12] = 0;


    if (max_basic >= 1) {
        cpuid(1, 0, &a, &b, &c, &d);
        info->stepping = a & 0xF;
        info->model    = (a >> 4) & 0xF;
        info->family   = (a >> 8) & 0xF;
        if (info->family == 0xF) info->family += (a >> 20) & 0xFF;
        if (info->family == 6 || info->family == 0xF)
            info->model |= ((a >> 16) & 0xF) << 4;

        info->has_sse2    = (d >> 26) & 1;
        info->has_aes_ni  = (c >> 25) & 1;
        info->has_avx     = (c >> 28) & 1;
        info->has_rdrand  = (c >> 30) & 1;
        info->has_pcid    = (c >> 17) & 1;
    }
    if (max_basic >= 7) {
        cpuid(7, 0, &a, &b, &c, &d);
        info->has_avx2   = (b >>  5) & 1;
        info->has_sha    = (b >> 29) & 1;
        info->has_rdseed = (b >> 18) & 1;
    }


    cpuid(0x80000000, 0, &a, &b, &c, &d);
    if (a >= 0x80000004) {
        for (int leaf = 0; leaf < 3; leaf++) {
            cpuid(0x80000002 + leaf, 0, &a, &b, &c, &d);
            uint32_t off = leaf * 16;
            uint32_t r[4] = { a, b, c, d };
            for (int i = 0; i < 4; i++)
                for (int j = 0; j < 4; j++)
                    info->brand[off + i*4 + j] = (char)(r[i] >> (j * 8));
        }
        info->brand[48] = 0;
    }
}

uint32_t hal_memory_map(hal_mem_range_t* out, uint32_t max) {
    (void)out; (void)max; return 0;
}
uint64_t hal_total_usable_ram(void) { return 0; }

void  hal_irq_init(void) {}
void  hal_irq_enable(uint32_t v)  { (void)v; }
void  hal_irq_disable(uint32_t v) { (void)v; }
void  hal_irq_eoi(uint32_t v)     { (void)v; }
void  hal_mmu_init(void) {}
void* hal_map_phys(uint64_t p, uint64_t l, uint32_t f) { (void)l; (void)f; return (void*)(uintptr_t)p; }
void  hal_unmap(void* v, uint64_t l) { (void)v; (void)l; }
