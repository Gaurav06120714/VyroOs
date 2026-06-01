#include "smp.h"

// ─────────────────────────────────────────────────
// ACPI structures (packed)
// ─────────────────────────────────────────────────
typedef struct {
    char     signature[8];   // "RSD PTR "
    uint8_t  checksum;
    char     oemid[6];
    uint8_t  revision;
    uint32_t rsdt_addr;
    uint32_t length;
    uint64_t xsdt_addr;
    uint8_t  ext_checksum;
    uint8_t  reserved[3];
} __attribute__((packed)) rsdp_t;

typedef struct {
    char     signature[4];
    uint32_t length;
    uint8_t  revision;
    uint8_t  checksum;
    char     oemid[6];
    char     oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed)) sdt_header_t;

typedef struct {
    sdt_header_t header;
    uint32_t     lapic_address;
    uint32_t     flags;
    // followed by variable-length entries
} __attribute__((packed)) madt_t;

// MADT entry header
typedef struct {
    uint8_t type;
    uint8_t length;
} __attribute__((packed)) madt_entry_t;

// State
static uint8_t  apic_ids[MAX_CPUS];
static uint32_t num_cpus = 0;
static uint64_t lapic_addr = 0;
static uint8_t  found_acpi = 0;

// ─────────────────────────────────────────────────
// memcmp helper
// ─────────────────────────────────────────────────
static int sig_match(const char* a, const char* b, int n) {
    for (int i = 0; i < n; i++) if (a[i] != b[i]) return 0;
    return 1;
}

// ─────────────────────────────────────────────────
// find_rsdp: scan the BIOS area for "RSD PTR "
// ─────────────────────────────────────────────────
static rsdp_t* find_rsdp() {
    // Scan 0xE0000 - 0xFFFFF on 16-byte boundaries
    for (uint64_t addr = 0xE0000; addr < 0x100000; addr += 16) {
        const char* p = (const char*) addr;
        if (sig_match(p, "RSD PTR ", 8)) {
            return (rsdp_t*) addr;
        }
    }
    return 0;
}

// ─────────────────────────────────────────────────
// parse_madt: walk MADT entries, count Local APICs
// ─────────────────────────────────────────────────
static void parse_madt(madt_t* madt) {
    lapic_addr = madt->lapic_address;

    uint8_t* ptr = (uint8_t*)madt + sizeof(madt_t);
    uint8_t* end = (uint8_t*)madt + madt->header.length;

    while (ptr < end) {
        madt_entry_t* e = (madt_entry_t*) ptr;
        if (e->length == 0) break;

        if (e->type == 0) {   // Processor Local APIC
            // payload: acpi_proc_id(1), apic_id(1), flags(4)
            uint8_t  apic_id = ptr[3];
            uint32_t flags   = *(uint32_t*)(ptr + 4);
            if ((flags & 1) && num_cpus < MAX_CPUS) {   // enabled
                apic_ids[num_cpus++] = apic_id;
            }
        }
        ptr += e->length;
    }
}

// ─────────────────────────────────────────────────
// scan_sdt: find the MADT ("APIC") in RSDT/XSDT
// ─────────────────────────────────────────────────
static void scan_tables(rsdp_t* rsdp) {
    if (rsdp->revision >= 2 && rsdp->xsdt_addr) {
        // ACPI 2.0+: use XSDT (64-bit pointers)
        sdt_header_t* xsdt = (sdt_header_t*)(uint64_t)rsdp->xsdt_addr;
        uint32_t entries = (xsdt->length - sizeof(sdt_header_t)) / 8;
        uint64_t* ptrs = (uint64_t*)((uint8_t*)xsdt + sizeof(sdt_header_t));
        for (uint32_t i = 0; i < entries; i++) {
            sdt_header_t* t = (sdt_header_t*) ptrs[i];
            if (sig_match(t->signature, "APIC", 4)) { parse_madt((madt_t*)t); return; }
        }
    } else {
        // ACPI 1.0: use RSDT (32-bit pointers)
        sdt_header_t* rsdt = (sdt_header_t*)(uint64_t)rsdp->rsdt_addr;
        uint32_t entries = (rsdt->length - sizeof(sdt_header_t)) / 4;
        uint32_t* ptrs = (uint32_t*)((uint8_t*)rsdt + sizeof(sdt_header_t));
        for (uint32_t i = 0; i < entries; i++) {
            sdt_header_t* t = (sdt_header_t*)(uint64_t)ptrs[i];
            if (sig_match(t->signature, "APIC", 4)) { parse_madt((madt_t*)t); return; }
        }
    }
}

// ─────────────────────────────────────────────────
// smp_init: detect CPU cores via ACPI
// ─────────────────────────────────────────────────
void smp_init() {
    num_cpus   = 0;
    lapic_addr = 0;
    found_acpi = 0;

    rsdp_t* rsdp = find_rsdp();
    if (!rsdp) {
        num_cpus = 1;   // assume single core if no ACPI
        return;
    }
    found_acpi = 1;
    scan_tables(rsdp);

    if (num_cpus == 0) num_cpus = 1;
}

uint32_t cpu_count()              { return num_cpus; }
uint8_t  cpu_apic_id(uint32_t i)  { return (i < num_cpus) ? apic_ids[i] : 0; }
uint64_t lapic_address()          { return lapic_addr; }
uint8_t  acpi_found()             { return found_acpi; }
