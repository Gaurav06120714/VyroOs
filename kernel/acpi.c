#include "acpi.h"

#define MAX_TABLES 32
static const acpi_sdt_t* tables[MAX_TABLES];
static uint32_t          n_tables = 0;
static const acpi_rsdp_t* rsdp = 0;

static int sig_eq(const char* a, const char* b) {
    for (int i = 0; i < 4; i++) if (a[i] != b[i]) return 0;
    return 1;
}

static int sig8_eq(const char* a, const char* b) {
    for (int i = 0; i < 8; i++) if (a[i] != b[i]) return 0;
    return 1;
}

static uint8_t checksum(const void* p, uint32_t n) {
    const uint8_t* b = (const uint8_t*)p;
    uint32_t s = 0;
    for (uint32_t i = 0; i < n; i++) s += b[i];
    return (uint8_t)s;
}

const acpi_rsdp_t* acpi_find_rsdp(void) {


    const char SIG[8] = { 'R','S','D',' ','P','T','R',' ' };



    for (uint64_t addr = 0xE0000; addr < 0x100000; addr += 16) {
        const acpi_rsdp_t* r = (const acpi_rsdp_t*)(uintptr_t)addr;
        if (sig8_eq(r->signature, SIG)) {

            if (checksum(r, 20) == 0) return r;
        }
    }
    return 0;
}

int acpi_init(void) {
    n_tables = 0;
    rsdp = acpi_find_rsdp();
    if (!rsdp) return 0;

    if (rsdp->revision >= 2 && rsdp->xsdt_address) {
        const acpi_sdt_t* xsdt = (const acpi_sdt_t*)(uintptr_t)rsdp->xsdt_address;
        uint32_t entry_count = (xsdt->length - sizeof(acpi_sdt_t)) / 8;
        const uint64_t* entries = (const uint64_t*)((const uint8_t*)xsdt + sizeof(acpi_sdt_t));
        for (uint32_t i = 0; i < entry_count && n_tables < MAX_TABLES; i++) {
            tables[n_tables++] = (const acpi_sdt_t*)(uintptr_t)entries[i];
        }
    } else if (rsdp->rsdt_address) {
        const acpi_sdt_t* rsdt = (const acpi_sdt_t*)(uintptr_t)rsdp->rsdt_address;
        uint32_t entry_count = (rsdt->length - sizeof(acpi_sdt_t)) / 4;
        const uint32_t* entries = (const uint32_t*)((const uint8_t*)rsdt + sizeof(acpi_sdt_t));
        for (uint32_t i = 0; i < entry_count && n_tables < MAX_TABLES; i++) {
            tables[n_tables++] = (const acpi_sdt_t*)(uintptr_t)entries[i];
        }
    }
    return n_tables > 0;
}

const acpi_sdt_t* acpi_find_table(const char sig[4]) {
    for (uint32_t i = 0; i < n_tables; i++) {
        if (tables[i] && sig_eq(tables[i]->sig, sig)) return tables[i];
    }
    return 0;
}

uint32_t acpi_table_count(void) { return n_tables; }

uint32_t acpi_walk_madt(acpi_lapic_t* lapics_out, uint32_t lapic_max,
                        acpi_ioapic_t* ioapics_out, uint32_t ioapic_max) {
    const acpi_sdt_t* madt = acpi_find_table("APIC");
    if (!madt) return 0;
    const uint8_t* p = (const uint8_t*)madt + sizeof(acpi_sdt_t) + 8;
    const uint8_t* end = (const uint8_t*)madt + madt->length;
    uint32_t nl = 0, ni = 0;
    while (p + 2 <= end) {
        uint8_t type = p[0];
        uint8_t len  = p[1];
        if (len < 2 || p + len > end) break;
        if (type == MADT_LAPIC && len >= 8 && nl < lapic_max) {
            lapics_out[nl].apic_id = p[3];
            lapics_out[nl].enabled = p[4] & 1;
            nl++;
        } else if (type == MADT_IOAPIC && len >= 12 && ni < ioapic_max) {
            ioapics_out[ni].ioapic_id = p[2];
            ioapics_out[ni].address   = (uint32_t)p[4] | ((uint32_t)p[5] << 8)
                                       | ((uint32_t)p[6] << 16) | ((uint32_t)p[7] << 24);
            ioapics_out[ni].gsi_base  = (uint32_t)p[8] | ((uint32_t)p[9] << 8)
                                       | ((uint32_t)p[10] << 16) | ((uint32_t)p[11] << 24);
            ni++;
        }
        p += len;
    }
    return (nl << 16) | ni;
}

void acpi_dump_tables(void (*println)(const char*)) {
    char buf[80];
    for (uint32_t i = 0; i < n_tables; i++) {
        const acpi_sdt_t* t = tables[i];
        if (!t) continue;
        int p = 0;
        buf[p++] = ' '; buf[p++] = ' ';
        for (int j = 0; j < 4; j++) buf[p++] = t->sig[j];
        buf[p++] = ' '; buf[p++] = 'l'; buf[p++] = 'e'; buf[p++] = 'n'; buf[p++] = '=';
        uint32_t l = t->length;
        char num[12]; int nl = 0;
        if (l == 0) num[nl++] = '0';
        while (l > 0) { num[nl++] = '0' + (l % 10); l /= 10; }
        while (nl > 0) buf[p++] = num[--nl];
        buf[p++] = ' '; buf[p++] = 'o'; buf[p++] = 'e'; buf[p++] = 'm'; buf[p++] = '=';
        for (int j = 0; j < 6; j++) buf[p++] = t->oem_id[j];
        buf[p] = 0;
        println(buf);
    }
}
