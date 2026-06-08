#ifndef ACPI_H
#define ACPI_H

#include "../include/types.h"

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
    char     signature[8];
    uint8_t  checksum;
    char     oem_id[6];
    uint8_t  revision;
    uint32_t rsdt_address;
    uint32_t length;
    uint64_t xsdt_address;
    uint8_t  extended_checksum;
    uint8_t  reserved[3];
} __attribute__((packed)) acpi_rsdp_t;

const acpi_rsdp_t* acpi_find_rsdp(void);

int acpi_init(void);

const acpi_sdt_t* acpi_find_table(const char sig[4]);

uint32_t acpi_table_count(void);

void acpi_dump_tables(void (*println)(const char*));

#define MADT_LAPIC   0
#define MADT_IOAPIC  1

typedef struct {
    uint8_t  apic_id;
    uint8_t  enabled;
} acpi_lapic_t;

typedef struct {
    uint8_t  ioapic_id;
    uint32_t address;
    uint32_t gsi_base;
} acpi_ioapic_t;

#define ACPI_MAX_LAPIC   64
#define ACPI_MAX_IOAPIC   8

uint32_t acpi_walk_madt(acpi_lapic_t* lapics_out, uint32_t lapic_max,
                        acpi_ioapic_t* ioapics_out, uint32_t ioapic_max);

#endif
