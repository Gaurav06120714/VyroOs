#ifndef ELF_H
#define ELF_H

#include "../include/types.h"

// ─────────────────────────────────────────────────
// ELF64 header
// ─────────────────────────────────────────────────
typedef struct {
    uint8_t  e_ident[16];   // magic + class/data/version
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;       // entry point virtual address
    uint64_t e_phoff;       // program header table offset
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;   // size of one program header
    uint16_t e_phnum;       // number of program headers
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} __attribute__((packed)) elf64_ehdr_t;

// ─────────────────────────────────────────────────
// ELF64 program header
// ─────────────────────────────────────────────────
typedef struct {
    uint32_t p_type;        // 1 = PT_LOAD
    uint32_t p_flags;
    uint64_t p_offset;      // offset in file
    uint64_t p_vaddr;       // virtual address to load at
    uint64_t p_paddr;
    uint64_t p_filesz;      // bytes in file
    uint64_t p_memsz;       // bytes in memory (>= filesz)
    uint64_t p_align;
} __attribute__((packed)) elf64_phdr_t;

#define PT_LOAD 1

// Loads an ELF image from memory. Returns the entry-point
// address, or 0 on failure.
uint64_t elf_load(const uint8_t* data, uint64_t size);

#endif
