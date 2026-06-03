#ifndef ACPI_H
#define ACPI_H

#include "../include/types.h"

// ACPI table walker. Finds RSDP (Root System Description Pointer) in the
// legacy BIOS area or via the UEFI configuration table, walks the RSDT/XSDT,
// and exposes pointers to the standard tables (FADT, MADT, HPET, DSDT, etc).
//
// Required to discover the real hardware on PC and Mac hardware:
//   - MADT  → list of LAPIC/IOAPIC entries (real interrupt controllers)
//   - FADT  → fixed ACPI feature info; pointer to DSDT (the AML to interpret)
//   - HPET  → high-precision event timer
//   - MCFG  → PCIe extended config (ECAM) base
// Battery, fan, thermals, sleep states all live inside DSDT and require an
// AML interpreter — that is its own multi-month phase.

#define ACPI_SIG(a,b,c,d) ((uint32_t)(a) | ((uint32_t)(b)<<8) | ((uint32_t)(c)<<16) | ((uint32_t)(d)<<24))

typedef struct {
    char     sig[4];
    uint32_t length;
    uint8_t  revision;
    uint8_t  checksum;
    char     oem_id[6];
    char     oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed)) acpi_sdt_t;

typedef struct {
    char     signature[8];        // "RSD PTR "
    uint8_t  checksum;
    char     oem_id[6];
    uint8_t  revision;
    uint32_t rsdt_address;
    uint32_t length;              // present if revision >= 2
    uint64_t xsdt_address;        // present if revision >= 2
    uint8_t  extended_checksum;
    uint8_t  reserved[3];
} __attribute__((packed)) acpi_rsdp_t;

// Scan low memory + EBDA for the RSDP signature. Returns NULL if not found.
const acpi_rsdp_t* acpi_find_rsdp(void);

// Initialise the walker — locates RSDP, parses RSDT/XSDT, stashes pointers
// to known tables. Returns 1 on success.
int acpi_init(void);

// Look up an SDT by 4-character signature ("FADT", "APIC", "HPET", "MCFG"…).
// Returns the table pointer or NULL if absent.
const acpi_sdt_t* acpi_find_table(const char sig[4]);

// Number of usable tables enumerated.
uint32_t acpi_table_count(void);

// Print a one-line summary of each table (sig, length, OEM id) to the shell.
void acpi_dump_tables(void (*println)(const char*));

#endif
