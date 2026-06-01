#ifndef SMP_H
#define SMP_H

#include "../include/types.h"

#define MAX_CPUS 32

void     smp_init();
uint32_t cpu_count();
uint8_t  cpu_apic_id(uint32_t index);
uint64_t lapic_address();
uint8_t  acpi_found();

#endif
