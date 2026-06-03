#include "../hal.h"

// ARM64 HAL — skeleton only. None of these are exercised by the current
// x86_64 build; the file exists so a future ARM64 cross-compile target has
// real symbols to land against. Once we add `make ARCH=arm64`, the build
// system will compile this file instead of hal_x86_64.c.

const char* hal_arch_name(void) { return "arm64"; }

uint64_t hal_cycle_counter(void) {
#if defined(__aarch64__)
    uint64_t v;
    __asm__ volatile("mrs %0, cntvct_el0" : "=r"(v));
    return v;
#else
    return 0;
#endif
}

void hal_cpu_detect(hal_cpu_info_t* info) {
    for (uint32_t i = 0; i < sizeof(*info); i++) ((uint8_t*)info)[i] = 0;
#if defined(__aarch64__)
    uint64_t midr;
    __asm__ volatile("mrs %0, midr_el1" : "=r"(midr));
    uint32_t implementer = (midr >> 24) & 0xFF;
    switch (implementer) {
    case 'A':  // 0x41 — ARM
        info->vendor[0]='A'; info->vendor[1]='R'; info->vendor[2]='M'; break;
    case 'P':  // 0x50 — Applied Micro
        info->vendor[0]='X'; info->vendor[1]='G'; info->vendor[2]='e'; info->vendor[3]='n'; break;
    case 'Q':  // 0x51 — Qualcomm
        info->vendor[0]='Q'; info->vendor[1]='C'; info->vendor[2]='O'; info->vendor[3]='M'; break;
    case 0x61: // Apple
        info->vendor[0]='A'; info->vendor[1]='p'; info->vendor[2]='p';
        info->vendor[3]='l'; info->vendor[4]='e'; break;
    default:
        info->vendor[0]='?'; break;
    }
    info->family   = (midr >> 16) & 0xF;
    info->model    = (midr >>  4) & 0xFFF;
    info->stepping = midr & 0xF;
#endif
}

uint32_t hal_memory_map(hal_mem_range_t* out, uint32_t max) { (void)out; (void)max; return 0; }
uint64_t hal_total_usable_ram(void) { return 0; }
void  hal_irq_init(void) {}
void  hal_irq_enable(uint32_t v)  { (void)v; }
void  hal_irq_disable(uint32_t v) { (void)v; }
void  hal_irq_eoi(uint32_t v)     { (void)v; }
void  hal_mmu_init(void) {}
void* hal_map_phys(uint64_t p, uint64_t l, uint32_t f) { (void)l; (void)f; return (void*)(uintptr_t)p; }
void  hal_unmap(void* v, uint64_t l) { (void)v; (void)l; }
