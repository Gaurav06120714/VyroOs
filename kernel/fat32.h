#ifndef FAT32_H
#define FAT32_H

#include "../include/types.h"

typedef struct {
    char     name[12];
    uint8_t  attr;
    uint32_t first_cluster;
    uint32_t size;
} fat32_dirent_t;

int fat32_mount(void);

int fat32_mount_block(uint32_t block_idx);

void fat32_use_block(int yes, uint32_t block_idx);

int fat32_is_mounted(void);

int fat32_list_root(fat32_dirent_t* out, int max);

int fat32_read_file(const char* name, uint8_t* out, uint32_t max_bytes);

int fat32_path_lookup(const char* path, fat32_dirent_t* out);

int fat32_list_cluster(uint32_t cluster, fat32_dirent_t* out, int max);

int fat32_write_file(const char* name, const uint8_t* data, uint32_t len);

#endif
