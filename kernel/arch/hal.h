#ifndef ARCH_HAL_H
#define ARCH_HAL_H

#include "../../include/types.h"

// Hardware Abstraction Layer — minimum surface that every architecture must
// implement. Today only x86_64 is fully wired; ARM64 has stub files that
// build but don't run. This header is the contract.

// CPU identity / features
typedef struct {
    char     vendor[16];           // "GenuineIntel" / "AuthenticAMD" / "Apple" / etc.
    char     brand[64];            // human-readable CPU brand
    uint32_t family;
    uint32_t model;
    uint32_t stepping;
    uint8_t  has_sse2;
    uint8_t  has_avx;
    uint8_t  has_avx2;
    uint8_t  has_aes_ni;
    uint8_t  has_sha;
    uint8_t  has_rdrand;
    uint8_t  has_rdseed;
    uint8_t  has_pcid;
} hal_cpu_info_t;

void hal_cpu_detect(hal_cpu_info_t* info);

// Memory map: an architecture-neutral view of the firmware-provided table.
// On x86_64 this comes from BIOS E820 or UEFI GetMemoryMap; on ARM64 it
// comes from the device tree's `/memory` node or UEFI.
#define HAL_MEM_USABLE       1
#define HAL_MEM_RESERVED     2
#define HAL_MEM_ACPI_RECLAIM 3
#define HAL_MEM_ACPI_NVS     4
#define HAL_MEM_BAD          5

typedef struct {
    uint64_t base;
    uint64_t length;
    uint32_t type;
} hal_mem_range_t;

#define HAL_MEM_MAX_RANGES 64
uint32_t hal_memory_map(hal_mem_range_t* out, uint32_t max);
uint64_t hal_total_usable_ram(void);

// Architecture-specific IRQ controller hookup (PIC/IOAPIC on x86, GIC/AIC on ARM)
void hal_irq_init(void);
void hal_irq_enable(uint32_t vector);
void hal_irq_disable(uint32_t vector);
void hal_irq_eoi(uint32_t vector);

// Page table / MMU primitives
void  hal_mmu_init(void);
void* hal_map_phys(uint64_t phys, uint64_t length, uint32_t flags);
void  hal_unmap(void* virt, uint64_t length);

// Atomic timer reading
uint64_t hal_cycle_counter(void);   // rdtsc on x86, CNTVCT_EL0 on ARM

// Architecture name for boot banner
const char* hal_arch_name(void);

#endif
