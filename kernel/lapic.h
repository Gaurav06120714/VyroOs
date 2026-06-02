#ifndef LAPIC_H
#define LAPIC_H

#include "../include/types.h"

// Local APIC initialization and access.
// Default MMIO base is 0xFEE00000 (per Intel SDM Vol. 3A). Identity-mapped
// by the kernel's 4 GiB paging, so direct reads/writes work.
//
// AP bring-up (INIT-SIPI trampoline) is a separate follow-up phase — this
// module ships the LAPIC enable + ICR send primitives needed for it.

#define LAPIC_DEFAULT_BASE   0xFEE00000ULL

#define LAPIC_REG_ID         0x020
#define LAPIC_REG_VERSION    0x030
#define LAPIC_REG_TPR        0x080
#define LAPIC_REG_EOI        0x0B0
#define LAPIC_REG_SVR        0x0F0
#define LAPIC_REG_ICR_LO     0x300
#define LAPIC_REG_ICR_HI     0x310

int      lapic_init(void);                  // returns 1 on success
int      lapic_is_enabled(void);
uint64_t lapic_base(void);
uint32_t lapic_id(void);
uint32_t lapic_version(void);

// Send an Inter-Processor Interrupt. `dest_apic_id` is the target LAPIC ID.
// `icr_lo` carries delivery mode, vector, and assert flags per Intel SDM.
void     lapic_send_ipi(uint32_t dest_apic_id, uint32_t icr_lo);

// Signal end-of-interrupt to the LAPIC (no-op if LAPIC not initialized).
void     lapic_eoi(void);

#endif
