#ifndef PMM_H
#define PMM_H

#include "../include/types.h"

#define PAGE_SIZE       4096
#define PMM_BITMAP_ADDR 0x100000   // Bitmap lives at 1MB mark
#define PMM_MEM_START   0x200000   // Free pages start at 2MB
// PMM_MEM_END is now derived from the memmap at runtime; legacy callers
// still get a sensible default ceiling.
#define PMM_MEM_END_DEFAULT 0x4000000  // 64MB fallback if memmap is empty

#define PMM_TOTAL_PAGES_DEFAULT ((PMM_MEM_END_DEFAULT - PMM_MEM_START) / PAGE_SIZE)

// Legacy macro kept for code paths that haven't migrated to pmm_total_pages().
#define PMM_TOTAL_PAGES  PMM_TOTAL_PAGES_DEFAULT

void     pmm_init();
void*    pmm_alloc_page();
void     pmm_free_page(void* addr);
uint32_t pmm_used_pages();
uint32_t pmm_free_pages();
uint32_t pmm_total_pages();

// vC.6.6: rebuild the bitmap from the parsed memory map. Marks every
// MEMMAP_USABLE range below the configured ceiling as free, every other
// kind (RESERVED, ACPI_NVS, BOOT_*, BAD) as already-used. Call AFTER
// memmap_init_from_e820 / memmap_init_from_uefi has populated memmap.h.
// Returns the new total page count.
uint32_t pmm_apply_memmap(void);

#endif
