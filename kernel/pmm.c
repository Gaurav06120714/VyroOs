#include "pmm.h"
#include "memmap.h"

static uint8_t* bitmap = (uint8_t*) PMM_BITMAP_ADDR;
static uint32_t used_count   = 0;
static uint32_t g_total_pages = PMM_TOTAL_PAGES_DEFAULT;

static void bitmap_set(uint32_t page) {
    bitmap[page / 8] |= (1 << (page % 8));
}

static void bitmap_clear(uint32_t page) {
    bitmap[page / 8] &= ~(1 << (page % 8));
}

static uint8_t bitmap_test(uint32_t page) {
    return bitmap[page / 8] & (1 << (page % 8));
}

static void mark_range(uint64_t base, uint64_t length, int busy) {
    if (length == 0) return;
    if (base < PMM_MEM_START) {
        uint64_t skip = PMM_MEM_START - base;
        if (skip >= length) return;
        base   += skip;
        length -= skip;
    }
    uint64_t end = base + length;
    uint32_t first_page = (uint32_t)((base - PMM_MEM_START) / PAGE_SIZE);
    uint32_t last_page  = (uint32_t)((end  - PMM_MEM_START + PAGE_SIZE - 1) / PAGE_SIZE);
    if (last_page > g_total_pages) last_page = g_total_pages;
    for (uint32_t p = first_page; p < last_page; p++) {
        if (busy) {
            if (!bitmap_test(p)) { bitmap_set(p); used_count++; }
        } else {
            if (bitmap_test(p))  { bitmap_clear(p); if (used_count) used_count--; }
        }
    }
}

void pmm_init() {
    g_total_pages = PMM_TOTAL_PAGES_DEFAULT;
    uint32_t bitmap_bytes = g_total_pages / 8 + 1;
    for (uint32_t i = 0; i < bitmap_bytes; i++) bitmap[i] = 0;
    used_count = 0;
}

uint32_t pmm_apply_memmap(void) {
    const memmap_t *mm = memmap_get();
    if (!mm || !mm->initialized) return g_total_pages;






    uint64_t cap_end = PMM_MEM_END_DEFAULT;
    if (mm->highest_usable && mm->highest_usable < cap_end) cap_end = mm->highest_usable;
    if (cap_end <= PMM_MEM_START) return 0;

    g_total_pages = (uint32_t)((cap_end - PMM_MEM_START) / PAGE_SIZE);


    uint32_t bitmap_bytes = g_total_pages / 8 + 1;
    for (uint32_t i = 0; i < bitmap_bytes; i++) bitmap[i] = 0xFF;
    used_count = g_total_pages;


    for (uint32_t i = 0; i < mm->count; i++) {
        if (mm->entries[i].kind == MEMMAP_USABLE) {
            mark_range(mm->entries[i].base, mm->entries[i].length, 0);
        }
    }






    for (uint32_t i = 0; i < mm->count; i++) {
        uint32_t k = mm->entries[i].kind;
        if (k == MEMMAP_RESERVED || k == MEMMAP_ACPI_NVS || k == MEMMAP_BAD) {
            mark_range(mm->entries[i].base, mm->entries[i].length, 1);
        }
    }




    if (g_total_pages > 0) { if (!bitmap_test(0)) { bitmap_set(0); used_count++; } }

    return g_total_pages;
}

void* pmm_alloc_page() {
    for (uint32_t i = 0; i < g_total_pages; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            used_count++;
            return (void*)((uint64_t)PMM_MEM_START + (uint64_t)i * PAGE_SIZE);
        }
    }
    return 0;
}

void pmm_free_page(void* addr) {
    uint32_t page = ((uint32_t)(uint64_t)addr - PMM_MEM_START) / PAGE_SIZE;
    if (page < g_total_pages && bitmap_test(page)) {
        bitmap_clear(page);
        if (used_count > 0) used_count--;
    }
}

uint32_t pmm_used_pages()  { return used_count; }
uint32_t pmm_free_pages()  { return g_total_pages - used_count; }
uint32_t pmm_total_pages() { return g_total_pages; }
