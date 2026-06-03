#include "pmm.h"
#include "memmap.h"

// Bitmap stored at 0x100000. Each bit represents one 4KB page starting
// from PMM_MEM_START.
static uint8_t* bitmap = (uint8_t*) PMM_BITMAP_ADDR;
static uint32_t used_count   = 0;
static uint32_t g_total_pages = PMM_TOTAL_PAGES_DEFAULT;

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

// Mark a contiguous physical range as used (busy=1) or free (busy=0).
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

// ─────────────────────────────────────────────────
// pmm_init: zero the bitmap (all pages free) — legacy entry point.
// Caller is expected to follow with pmm_apply_memmap() once memmap has
// been built, but this preserves the v1.0–v6.0 behavior for code paths
// that haven't been updated yet.
// ─────────────────────────────────────────────────
void pmm_init() {
    g_total_pages = PMM_TOTAL_PAGES_DEFAULT;
    uint32_t bitmap_bytes = g_total_pages / 8 + 1;
    for (uint32_t i = 0; i < bitmap_bytes; i++) bitmap[i] = 0;
    used_count = 0;
}

// vC.6.6: rebuild the bitmap from the parsed memory map.
uint32_t pmm_apply_memmap(void) {
    const memmap_t *mm = memmap_get();
    if (!mm || !mm->initialized) return g_total_pages;

    // Decide the new ceiling: min(highest_usable, our static cap so we
    // don't outgrow the bitmap region at 0x100000). The static cap is
    // determined by where PMM_MEM_END_DEFAULT historically lived; in a
    // post-v7 world this should grow with available RAM, but we keep
    // the conservative ceiling so the bitmap fits in its 1MB slot.
    uint64_t cap_end = PMM_MEM_END_DEFAULT;
    if (mm->highest_usable && mm->highest_usable < cap_end) cap_end = mm->highest_usable;
    if (cap_end <= PMM_MEM_START) return 0;

    g_total_pages = (uint32_t)((cap_end - PMM_MEM_START) / PAGE_SIZE);

    // Start with everything marked busy. We'll release MEMMAP_USABLE ranges.
    uint32_t bitmap_bytes = g_total_pages / 8 + 1;
    for (uint32_t i = 0; i < bitmap_bytes; i++) bitmap[i] = 0xFF;
    used_count = g_total_pages;

    // Free every usable range that the firmware reported.
    for (uint32_t i = 0; i < mm->count; i++) {
        if (mm->entries[i].kind == MEMMAP_USABLE) {
            mark_range(mm->entries[i].base, mm->entries[i].length, /*busy=*/0);
        }
    }

    // Re-occupy ranges the firmware called reserved/ACPI-NVS/BAD that
    // happen to overlap our region. The all-busy start handles the
    // common case where the firmware never reported them, but we still
    // need to handle the case where a usable range and a reserved range
    // overlap and the firmware listed usable last.
    for (uint32_t i = 0; i < mm->count; i++) {
        uint32_t k = mm->entries[i].kind;
        if (k == MEMMAP_RESERVED || k == MEMMAP_ACPI_NVS || k == MEMMAP_BAD) {
            mark_range(mm->entries[i].base, mm->entries[i].length, /*busy=*/1);
        }
    }

    // Always reserve the kernel image + bitmap area itself
    // (everything below PMM_MEM_START is implicitly busy because the
    // bitmap doesn't cover it). Belt-and-braces: reserve page 0 too.
    if (g_total_pages > 0) { if (!bitmap_test(0)) { bitmap_set(0); used_count++; } }

    return g_total_pages;
}

// ─────────────────────────────────────────────────
// pmm_alloc_page: find first free page and allocate
// Returns physical address or 0 if out of memory
// ─────────────────────────────────────────────────
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

// ─────────────────────────────────────────────────
// pmm_free_page: release a previously allocated page
// ─────────────────────────────────────────────────
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
