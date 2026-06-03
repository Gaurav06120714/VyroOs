#ifndef SMP_BOOT_H
#define SMP_BOOT_H

#include "../include/types.h"

// SMP AP bring-up via LAPIC INIT-SIPI-SIPI sequence.
//
// Trampoline: a tiny 16-bit real-mode stub planted at TRAMP_PHYS (0x8000).
// On SIPI it runs, increments a shared byte at FLAG_PHYS (0x9000), then
// HLTs in real mode. This is a foundation phase: it proves we can wake
// application processors. Full long-mode AP entry + per-CPU scheduling
// is a separate future phase.

#define SMP_TRAMP_PHYS  0x8000
#define SMP_FLAG_PHYS   0x9000

void smp_start_aps(void);
uint32_t smp_ap_count(void);

// Number of APs that have reached C (called ap_main).
uint32_t smp_ap_in_c_count(void);

// Per-CPU presence map: 1 byte per APIC ID (up to 256).
const uint8_t* smp_ap_in_c_map(void);

#endif
