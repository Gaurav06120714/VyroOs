#include "memmap.h"

static memmap_t g_mm;

// --- E820 entry layout (24 bytes, packed) ---
//   uint64_t base, uint64_t length, uint32_t type, uint32_t acpi_attr
typedef struct __attribute__((packed)) {
    uint64_t base;
    uint64_t length;
    uint32_t type;
    uint32_t acpi_attr;
} e820_entry_t;

// E820 type to our normalized kind
static uint32_t e820_kind(uint32_t t) {
    switch (t) {
    case 1: return MEMMAP_USABLE;
    case 2: return MEMMAP_RESERVED;
    case 3: return MEMMAP_ACPI_RECLAIM;
    case 4: return MEMMAP_ACPI_NVS;
    case 5: return MEMMAP_BAD;
    default: return MEMMAP_RESERVED;
    }
}

// UEFI memory type values from the spec
// 0=Reserved,1=LoaderCode,2=LoaderData,3=BootServicesCode,4=BootServicesData
// 5=RuntimeCode,6=RuntimeData,7=ConventionalMemory,8=Unusable,9=ACPIReclaim
// 10=ACPINVS,11=MMIO,12=MMIOPortSpace,13=PalCode,14=Persistent
static uint32_t uefi_kind(uint32_t t) {
    switch (t) {
    case 7:  return MEMMAP_USABLE;       // ConventionalMemory
    case 1:  case 2:  return MEMMAP_USABLE; // Loader code/data is ours post-handoff
    case 3:  return MEMMAP_BOOT_CODE;
    case 4:  return MEMMAP_BOOT_DATA;
    case 9:  return MEMMAP_ACPI_RECLAIM;
    case 10: return MEMMAP_ACPI_NVS;
    case 8:  return MEMMAP_BAD;
    default: return MEMMAP_RESERVED;
    }
}

static void recompute_totals(void) {
    g_mm.total_usable   = 0;
    g_mm.highest_usable = 0;
    for (uint32_t i = 0; i < g_mm.count; i++) {
        if (g_mm.entries[i].kind == MEMMAP_USABLE) {
            g_mm.total_usable += g_mm.entries[i].length;
            uint64_t top = g_mm.entries[i].base + g_mm.entries[i].length;
            if (top > g_mm.highest_usable) g_mm.highest_usable = top;
        }
    }
}

int memmap_init_from_e820(uint64_t bios_table_paddr) {
    // stage2 layout: u32 count at offset 0, entry array at offset 8.
    volatile uint32_t *count_ptr = (volatile uint32_t *)(uintptr_t)bios_table_paddr;
    uint32_t n = *count_ptr;
    if (!n || n > MEMMAP_MAX_ENTRIES) {
        if (!n) return 0;
        n = MEMMAP_MAX_ENTRIES;
    }
    e820_entry_t *src = (e820_entry_t *)(uintptr_t)(bios_table_paddr + 8);

    g_mm.count = 0;
    for (uint32_t i = 0; i < n; i++) {
        // Skip zero-length entries
        if (src[i].length == 0) continue;
        memmap_entry_t *e = &g_mm.entries[g_mm.count++];
        e->base   = src[i].base;
        e->length = src[i].length;
        e->kind   = e820_kind(src[i].type);
        e->reserved0 = 0;
    }
    g_mm.source_uefi = 0;
    g_mm.initialized = 1;
    recompute_totals();
    return 1;
}

int memmap_init_from_uefi(const void *descs, uint64_t count, uint64_t desc_size) {
    if (!descs || !count || desc_size < 32) return 0;
    if (count > MEMMAP_MAX_ENTRIES) count = MEMMAP_MAX_ENTRIES;

    const uint8_t *p = (const uint8_t *)descs;
    g_mm.count = 0;
    for (uint64_t i = 0; i < count; i++) {
        const uint8_t *d = p + i * desc_size;
        // EFI_MEMORY_DESCRIPTOR layout (V1, the only one in active use):
        //   u32 Type, u32 pad, u64 PhysicalStart, u64 VirtualStart, u64 NumberOfPages, u64 Attribute
        uint32_t type;
        uint64_t phys, pages;
        __builtin_memcpy(&type,  d + 0,  sizeof(type));
        __builtin_memcpy(&phys,  d + 8,  sizeof(phys));
        __builtin_memcpy(&pages, d + 24, sizeof(pages));
        if (!pages) continue;

        memmap_entry_t *e = &g_mm.entries[g_mm.count++];
        e->base   = phys;
        e->length = pages * 4096ull;
        e->kind   = uefi_kind(type);
        e->reserved0 = 0;
    }
    g_mm.source_uefi = 1;
    g_mm.initialized = 1;
    recompute_totals();
    return 1;
}

const memmap_t* memmap_get(void) { return &g_mm; }
