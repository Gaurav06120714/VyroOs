#ifndef ARCH_HAL_H
#define ARCH_HAL_H

#include "../../include/types.h"

typedef struct {
    char     vendor[16];
    char     brand[64];
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

void hal_irq_init(void);
void hal_irq_enable(uint32_t vector);
void hal_irq_disable(uint32_t vector);
void hal_irq_eoi(uint32_t vector);

void  hal_mmu_init(void);
void* hal_map_phys(uint64_t phys, uint64_t length, uint32_t flags);
void  hal_unmap(void* virt, uint64_t length);

uint64_t hal_cycle_counter(void);

const char* hal_arch_name(void);

#endif
