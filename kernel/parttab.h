#ifndef PARTTAB_H
#define PARTTAB_H

#include "../include/types.h"

// Partition table parser (vC.6.10).
//
// Walks both legacy MBR (DOS partition table at LBA 0, 4 primary entries
// at offset 0x1BE) and modern GPT (protective MBR at LBA 0, header at
// LBA 1, entry array starting at LBA 2). Exposes a unified view so the
// filesystem mount layer doesn't care which scheme the disk uses.
//
// vC.6.8/9 mounted FAT32 against the raw block device assuming the BPB
// sits at LBA 0 — that worked for whole-disk FAT32 images (USB sticks
// flashed by `dd`) but fails on partitioned disks where the BPB lives
// inside a partition starting at e.g. LBA 2048. parttab + a tiny
// partition-window block-device shim closes that gap.

typedef struct {
    int       in_use;
    uint8_t   type_id;            // MBR type byte, or GPT type GUID first byte
    uint64_t  start_lba;
    uint64_t  length_lba;
    char      name[36];           // GPT name (UTF-16 truncated to ASCII), or "p1".."p4" for MBR
} parttab_entry_t;

#define PARTTAB_MAX  8

typedef struct {
    int               initialized;
    int               source_gpt;          // 0=MBR, 1=GPT
    uint32_t          count;
    parttab_entry_t   entries[PARTTAB_MAX];
} parttab_t;

// Probe the block device for an MBR or GPT. Returns 1 on success.
int parttab_probe(uint32_t block_idx, parttab_t *out);

// Convenience: register a new block_device that windows into [start, length)
// of the underlying device. Returns the new block index, or -1 on failure.
int parttab_register_partition(uint32_t parent_idx, uint64_t start_lba,
                               uint64_t length_lba, const char *label);

#endif
