#ifndef LAPIC_H
#define LAPIC_H

#include "../include/types.h"

#define LAPIC_DEFAULT_BASE   0xFEE00000ULL

#define LAPIC_REG_ID         0x020
#define LAPIC_REG_VERSION    0x030
#define LAPIC_REG_TPR        0x080
#define LAPIC_REG_EOI        0x0B0
#define LAPIC_REG_SVR        0x0F0
#define LAPIC_REG_ICR_LO     0x300
#define LAPIC_REG_ICR_HI     0x310

int      lapic_init(void);
int      lapic_is_enabled(void);
uint64_t lapic_base(void);
uint32_t lapic_id(void);
uint32_t lapic_version(void);

void     lapic_send_ipi(uint32_t dest_apic_id, uint32_t icr_lo);

void     lapic_eoi(void);

#endif
