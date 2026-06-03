#include "smp_boot.h"
#include "lapic.h"
#include "../drivers/timer.h"

extern uint8_t  smp_trampoline_blob[];
extern uint32_t smp_trampoline_blob_len(void);

static inline uint64_t read_cr3(void) {
    uint64_t v;
    __asm__ volatile("mov %%cr3, %0" : "=r"(v));
    return v;
}

void smp_start_aps(void) {
    if (!lapic_is_enabled()) return;

    // Copy the assembled trampoline blob to physical 0x8000.
    volatile uint8_t* dst = (volatile uint8_t*)SMP_TRAMP_PHYS;
    uint32_t n = smp_trampoline_blob_len();
    for (uint32_t i = 0; i < n; i++) dst[i] = smp_trampoline_blob[i];

    // Publish the BSP's PML4 physical address at 0x9008 (after the AP counter
    // byte at 0x9000). The trampoline reads this when setting CR3.
    uint64_t pml4 = read_cr3() & ~0xFFFULL;
    *(volatile uint32_t*)(SMP_FLAG_PHYS + 8) = (uint32_t)pml4;
    *(volatile uint8_t*)SMP_FLAG_PHYS = 0;

    // INIT IPI broadcast to all-excluding-self
    lapic_send_ipi(0, 0x000C4500);
    sleep_ms(10);
    lapic_send_ipi(0, 0x00084500);            // de-assert
    sleep_ms(1);
    // SIPI ×2, vector = trampoline_page = 0x08
    lapic_send_ipi(0, 0x000C4600 | (SMP_TRAMP_PHYS >> 12));
    sleep_ms(1);
    lapic_send_ipi(0, 0x000C4600 | (SMP_TRAMP_PHYS >> 12));
    sleep_ms(100);
}

uint32_t smp_ap_count(void) {
    return *(volatile uint8_t*)SMP_FLAG_PHYS;
}
