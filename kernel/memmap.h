#ifndef MEMMAP_H
#define MEMMAP_H

#include "../include/types.h"

typedef enum {
    MEMMAP_USABLE      = 1,
    MEMMAP_RESERVED    = 2,
    MEMMAP_ACPI_RECLAIM= 3,
    MEMMAP_ACPI_NVS    = 4,
    MEMMAP_BAD         = 5,
    MEMMAP_BOOT_CODE   = 6,
    MEMMAP_BOOT_DATA   = 7,
} memmap_kind_t;

typedef struct {
    uint64_t base;
    uint64_t length;
    uint32_t kind;
    uint32_t reserved0;
} memmap_entry_t;

#define MEMMAP_MAX_ENTRIES 64

typedef struct {
    int             initialized;
    int             source_uefi;
    uint32_t        count;
    uint64_t        total_usable;
    uint64_t        highest_usable;
    memmap_entry_t  entries[MEMMAP_MAX_ENTRIES];
} memmap_t;

int memmap_init_from_e820(uint64_t bios_table_paddr);

int memmap_init_from_uefi(const void *descs, uint64_t count, uint64_t desc_size);

const memmap_t* memmap_get(void);

#endif
