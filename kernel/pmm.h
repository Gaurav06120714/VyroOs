#ifndef PMM_H
#define PMM_H

#include "../include/types.h"

#define PAGE_SIZE       4096
#define PMM_BITMAP_ADDR 0x100000   // Bitmap lives at 1MB mark
#define PMM_MEM_START   0x200000   // Free pages start at 2MB
#define PMM_MEM_END     0x4000000  // Manage up to 64MB (expandable)

#define PMM_TOTAL_PAGES ((PMM_MEM_END - PMM_MEM_START) / PAGE_SIZE)

void     pmm_init();
void*    pmm_alloc_page();
void     pmm_free_page(void* addr);
uint32_t pmm_used_pages();
uint32_t pmm_free_pages();
uint32_t pmm_total_pages();

#endif
