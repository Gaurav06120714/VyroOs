#ifndef MEMMAP_H
#define MEMMAP_H

#include "../include/types.h"

// Unified physical-memory map exposed to the kernel by either the
// BIOS E820 path (boot/stage2 hands us an array at a known low address)
// or the UEFI GetMemoryMap path (the UEFI stub hands us a UEFI mmap).
// vC.6.5 normalizes both into one in-memory representation that the PMM
// consumes to know which ranges it may hand out as RAM.

typedef enum {
    MEMMAP_USABLE      = 1,
    MEMMAP_RESERVED    = 2,
    MEMMAP_ACPI_RECLAIM= 3,
    MEMMAP_ACPI_NVS    = 4,
    MEMMAP_BAD         = 5,
    MEMMAP_BOOT_CODE   = 6,    // UEFI boot services code
    MEMMAP_BOOT_DATA   = 7,    // UEFI boot services data
} memmap_kind_t;

typedef struct {
    uint64_t base;
    uint64_t length;
    uint32_t kind;             // memmap_kind_t
    uint32_t reserved0;
} memmap_entry_t;

#define MEMMAP_MAX_ENTRIES 64

typedef struct {
    int             initialized;
    int             source_uefi;    // 0 = E820, 1 = UEFI
    uint32_t        count;
    uint64_t        total_usable;
    uint64_t        highest_usable;
    memmap_entry_t  entries[MEMMAP_MAX_ENTRIES];
} memmap_t;

// Parse E820 entries already gathered by the BIOS stage2 loader at the
// well-known low-mem address (0x9000, count at 0x9000+0, table at +8).
// Returns 1 on success.
int memmap_init_from_e820(uint64_t bios_table_paddr);

// Parse a UEFI memory map descriptor array. The UEFI stub passes
// (descriptors, count, descriptor_size) since EFI_MEMORY_DESCRIPTOR
// can grow without warning across versions.
int memmap_init_from_uefi(const void *descs, uint64_t count, uint64_t desc_size);

const memmap_t* memmap_get(void);

#endif
