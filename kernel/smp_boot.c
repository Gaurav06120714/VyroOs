#include "smp_boot.h"
#include "lapic.h"
#include "../drivers/timer.h"

extern uint8_t  smp_trampoline_blob[];
extern uint32_t smp_trampoline_blob_len(void);

static volatile uint8_t  aps_in_c[256];
static volatile uint64_t aps_in_c_count = 0;

void ap_main(uint32_t apic_id) {
    if (apic_id < 256) aps_in_c[apic_id] = 1;
    __sync_fetch_and_add(&aps_in_c_count, 1);
    while (1) __asm__ volatile("hlt");
}

uint32_t       smp_ap_in_c_count(void) { return (uint32_t)aps_in_c_count; }
const uint8_t* smp_ap_in_c_map(void)   { return (const uint8_t*)aps_in_c; }

static inline uint64_t read_cr3(void) {
    uint64_t v;
    __asm__ volatile("mov %%cr3, %0" : "=r"(v));
    return v;
}

void smp_start_aps(void) {
    if (!lapic_is_enabled()) return;


    volatile uint8_t* dst = (volatile uint8_t*)SMP_TRAMP_PHYS;
    uint32_t n = smp_trampoline_blob_len();
    for (uint32_t i = 0; i < n; i++) dst[i] = smp_trampoline_blob[i];



    uint64_t pml4 = read_cr3() & ~0xFFFULL;
    *(volatile uint32_t*)(SMP_FLAG_PHYS + 8) = (uint32_t)pml4;
    *(volatile uint64_t*)(SMP_FLAG_PHYS + 16) = (uint64_t)(uintptr_t)&ap_main;
    *(volatile uint8_t*)SMP_FLAG_PHYS = 0;
    for (int i = 0; i < 256; i++) aps_in_c[i] = 0;
    aps_in_c_count = 0;


    lapic_send_ipi(0, 0x000C4500);
    sleep_ms(10);
    lapic_send_ipi(0, 0x00084500);
    sleep_ms(1);

    lapic_send_ipi(0, 0x000C4600 | (SMP_TRAMP_PHYS >> 12));
    sleep_ms(1);
    lapic_send_ipi(0, 0x000C4600 | (SMP_TRAMP_PHYS >> 12));
    sleep_ms(100);
}

uint32_t smp_ap_count(void) {
    return *(volatile uint8_t*)SMP_FLAG_PHYS;
}
