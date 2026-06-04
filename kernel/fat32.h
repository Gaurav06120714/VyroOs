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
// Legacy entry: reads through ata_read_sector on the boot disk.
int fat32_mount(void);

// vC.6.8: bind the mount to a generic block device (block_get(idx)) and
// probe it for a FAT32 BPB. All subsequent FAT32 calls go through the
// block layer, so the same filesystem code reads SATA via AHCI or NVMe
// without caring which.
int fat32_mount_block(uint32_t block_idx);

// Flip the transport without re-running BPB parse. Exposed for the rare
// boot-init suspend/resume case.
void fat32_use_block(int yes, uint32_t block_idx);

int fat32_is_mounted(void);

// Read up to `max` entries from the root directory.
// Returns the number of entries written.
int fat32_list_root(fat32_dirent_t* out, int max);

// Read up to `max_bytes` from `name` (8.3) in the root dir.
// Returns bytes read, or -1 on not found.
int fat32_read_file(const char* name, uint8_t* out, uint32_t max_bytes);

// Walk a `/`-separated path (e.g. "/dir/sub/file.txt") and fill *out with the
// matching directory entry. Returns 1 on success.
int fat32_path_lookup(const char* path, fat32_dirent_t* out);

// List entries in any directory by its starting cluster.
int fat32_list_cluster(uint32_t cluster, fat32_dirent_t* out, int max);

// Create or overwrite a file in the root directory. Returns bytes written.
int fat32_write_file(const char* name, const uint8_t* data, uint32_t len);

#endif
