#ifndef PMM_H
#define PMM_H

#include "../include/types.h"

#define PAGE_SIZE       4096
#define PMM_BITMAP_ADDR 0x100000
#define PMM_MEM_START   0x200000

#define PMM_MEM_END_DEFAULT 0x4000000

#define PMM_TOTAL_PAGES_DEFAULT ((PMM_MEM_END_DEFAULT - PMM_MEM_START) / PAGE_SIZE)

#define PMM_TOTAL_PAGES  PMM_TOTAL_PAGES_DEFAULT

void     pmm_init();
void*    pmm_alloc_page();
void     pmm_free_page(void* addr);
uint32_t pmm_used_pages();
uint32_t pmm_free_pages();
uint32_t pmm_total_pages();

uint32_t pmm_apply_memmap(void);

#endif
