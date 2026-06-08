#include "elf.h"

static void ecopy(uint8_t* dst, const uint8_t* src, uint64_t n) {
    for (uint64_t i = 0; i < n; i++) dst[i] = src[i];
}
static void ezero(uint8_t* dst, uint64_t n) {
    for (uint64_t i = 0; i < n; i++) dst[i] = 0;
}

uint64_t elf_load(const uint8_t* data, uint64_t size) {
    if (size < sizeof(elf64_ehdr_t)) return 0;

    const elf64_ehdr_t* eh = (const elf64_ehdr_t*) data;


    if (eh->e_ident[0] != 0x7F || eh->e_ident[1] != 'E' ||
        eh->e_ident[2] != 'L'  || eh->e_ident[3] != 'F') {
        return 0;
    }


    if (eh->e_ident[4] != 2) return 0;


    for (uint16_t i = 0; i < eh->e_phnum; i++) {
        const elf64_phdr_t* ph =
            (const elf64_phdr_t*)(data + eh->e_phoff + (uint64_t)i * eh->e_phentsize);

        if (ph->p_type != PT_LOAD) continue;

        uint8_t* dest = (uint8_t*) ph->p_vaddr;


        ecopy(dest, data + ph->p_offset, ph->p_filesz);


        if (ph->p_memsz > ph->p_filesz) {
            ezero(dest + ph->p_filesz, ph->p_memsz - ph->p_filesz);
        }
    }

    return eh->e_entry;
}
