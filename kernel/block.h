#ifndef BLOCK_H
#define BLOCK_H

#include "../include/types.h"

// Generic block-device abstraction so filesystems (FAT32, VyFS) can sit
// on top of either AHCI/SATA or NVMe without caring which transport they
// landed on. Each device exposes a fixed logical block size (512 or 4096)
// and uses 64-bit LBAs throughout.

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
    uint32_t        index;          // disambiguates same-kind devices
    uint32_t        logical_block_size;
    uint64_t        total_blocks;
    char            model[48];      // best-effort human-readable identity
    void           *transport;      // backend cookie (port id, nsid, etc.)
    block_read_fn   read;
    block_write_fn  write;
} block_device_t;

#define BLOCK_MAX_DEVICES 8

// Probe the system. Discovers every active AHCI port (calls ahci_port_init)
// and the first NVMe namespace (calls nvme_admin_init + nvme_io_init), then
// registers a block_device for each that succeeded. Returns the count
// registered (0..BLOCK_MAX_DEVICES).
int block_probe(void);

// Lookup.
block_device_t* block_get(uint32_t idx);
int             block_count(void);

// Convenience wrappers that dispatch through the function pointers.
int block_read (uint32_t idx, uint64_t lba, uint32_t count, void *buf);
int block_write(uint32_t idx, uint64_t lba, uint32_t count, const void *buf);

#endif
