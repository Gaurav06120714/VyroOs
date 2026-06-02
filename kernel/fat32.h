#ifndef FAT32_H
#define FAT32_H

#include "../include/types.h"

// Minimal FAT32 reader sitting on top of ata_read_sector().
// Supports: BPB parse, FAT chain walk, root-directory enumerate, file read.
// Does NOT support: writes, long filenames (8.3 only), FAT12/FAT16.

typedef struct {
    char     name[12];          // 8.3, null-terminated, no padding
    uint8_t  attr;
    uint32_t first_cluster;
    uint32_t size;
} fat32_dirent_t;

// Probe the disk for a FAT32 BPB. Returns 1 on success.
int fat32_mount(void);

int fat32_is_mounted(void);

// Read up to `max` entries from the root directory.
// Returns the number of entries written.
int fat32_list_root(fat32_dirent_t* out, int max);

// Read up to `max_bytes` from `name` (8.3) in the root dir.
// Returns bytes read, or -1 on not found.
int fat32_read_file(const char* name, uint8_t* out, uint32_t max_bytes);

#endif
