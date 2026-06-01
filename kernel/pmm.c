#include "pmm.h"

// Bitmap stored at 0x100000
// Each bit represents one 4KB page starting from PMM_MEM_START
static uint8_t* bitmap = (uint8_t*) PMM_BITMAP_ADDR;
static uint32_t used_count = 0;

// ─────────────────────────────────────────────────
// Bitmap helpers
// ─────────────────────────────────────────────────
static void bitmap_set(uint32_t page) {
    bitmap[page / 8] |= (1 << (page % 8));
}

static void bitmap_clear(uint32_t page) {
    bitmap[page / 8] &= ~(1 << (page % 8));
}

static uint8_t bitmap_test(uint32_t page) {
    return bitmap[page / 8] & (1 << (page % 8));
}

// ─────────────────────────────────────────────────
// pmm_init: zero the bitmap (all pages free)
// ─────────────────────────────────────────────────
void pmm_init() {
    uint32_t bitmap_bytes = PMM_TOTAL_PAGES / 8 + 1;
    for (uint32_t i = 0; i < bitmap_bytes; i++) {
        bitmap[i] = 0;
    }
    used_count = 0;
}

// ─────────────────────────────────────────────────
// pmm_alloc_page: find first free page and allocate
// Returns physical address or 0 if out of memory
// ─────────────────────────────────────────────────
void* pmm_alloc_page() {
    for (uint32_t i = 0; i < PMM_TOTAL_PAGES; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            used_count++;
            return (void*)((uint64_t)PMM_MEM_START + (uint64_t)i * PAGE_SIZE);
        }
    }
    return 0;  // Out of memory
}

// ─────────────────────────────────────────────────
// pmm_free_page: release a previously allocated page
// ─────────────────────────────────────────────────
void pmm_free_page(void* addr) {
    uint32_t page = ((uint32_t)(uint64_t)addr - PMM_MEM_START) / PAGE_SIZE;
    if (page < PMM_TOTAL_PAGES && bitmap_test(page)) {
        bitmap_clear(page);
        if (used_count > 0) used_count--;
    }
}

uint32_t pmm_used_pages()  { return used_count; }
uint32_t pmm_free_pages()  { return PMM_TOTAL_PAGES - used_count; }
uint32_t pmm_total_pages() { return PMM_TOTAL_PAGES; }
