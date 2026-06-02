#include "smp_boot.h"
#include "lapic.h"
#include "../drivers/timer.h"

// 16-bit real-mode trampoline:
//   fa            cli
//   31 c0         xor   ax, ax
//   8e d8         mov   ds, ax
//   bb 00 90      mov   bx, 0x9000
//   f0 fe 07      lock inc byte [bx]      ; atomic across cores
//   f4            hlt
//   eb fd         jmp   $-3               ; back into hlt
//
// Total: 12 bytes. Position-independent; CS:IP after SIPI will be vector*0x100:0,
// which physically is 0x8000 — we land at the first byte.
static const uint8_t smp_tramp[] = {
    0xFA,
    0x31, 0xC0,
    0x8E, 0xD8,
    0xBB, 0x00, 0x90,
    0xF0, 0xFE, 0x07,
    0xF4,
    0xEB, 0xFD
};

void smp_start_aps(void) {
    if (!lapic_is_enabled()) return;

    // Copy trampoline to low physical memory (identity-mapped by BSP paging).
    volatile uint8_t* dst = (volatile uint8_t*)SMP_TRAMP_PHYS;
    for (uint32_t i = 0; i < sizeof(smp_tramp); i++) dst[i] = smp_tramp[i];
    *(volatile uint8_t*)SMP_FLAG_PHYS = 0;

    // INIT IPI broadcast to all-excluding-self:
    //   bits 18..19 = 11 (all excluding self)
    //   bits 14    = level=1
    //   bits 8..10 = INIT (0b101)
    // = 0x000C4500
    lapic_send_ipi(0, 0x000C4500);
    sleep_ms(10);

    // De-assert INIT (level=0)
    lapic_send_ipi(0, 0x00084500);
    sleep_ms(1);

    // SIPI: mode=110, vector = trampoline_page = 0x08
    lapic_send_ipi(0, 0x000C4600 | (SMP_TRAMP_PHYS >> 12));
    sleep_ms(1);
    // Second SIPI per Intel SDM recommendation
    lapic_send_ipi(0, 0x000C4600 | (SMP_TRAMP_PHYS >> 12));
    // Give APs time to wake and bump the counter
    sleep_ms(100);
}

uint32_t smp_ap_count(void) {
    return *(volatile uint8_t*)SMP_FLAG_PHYS;
}
