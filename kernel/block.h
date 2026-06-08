#ifndef BLOCK_H
#define BLOCK_H

#include "../include/types.h"

typedef enum {
    BLOCK_KIND_AHCI = 1,
    BLOCK_KIND_NVME = 2,
} block_kind_t;

struct block_device;

typedef int (*block_read_fn) (struct block_device *bd, uint64_t lba, uint32_t count, void *buf);
typedef int (*block_write_fn)(struct block_device *bd, uint64_t lba, uint32_t count, const void *buf);

typedef struct block_device {
    int             in_use;
    block_kind_t    kind;
    uint32_t        index;
    uint32_t        logical_block_size;
    uint64_t        total_blocks;
    char            model[48];
    void           *transport;
    block_read_fn   read;
    block_write_fn  write;
} block_device_t;

#define BLOCK_MAX_DEVICES 8

int block_probe(void);

block_device_t* block_get(uint32_t idx);
int             block_count(void);

int block_read (uint32_t idx, uint64_t lba, uint32_t count, void *buf);
int block_write(uint32_t idx, uint64_t lba, uint32_t count, const void *buf);

int block_register(block_device_t *src);

#endif
