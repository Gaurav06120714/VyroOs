#ifndef NVME_H
#define NVME_H

#include "../include/types.h"

typedef struct {
    int      present;
    uint64_t mmio_base;
    uint64_t cap;
    uint32_t version;
    uint32_t max_q_entries;
    uint8_t  doorbell_stride;


    int      admin_ready;
    char     serial[21];
    char     model[41];
    uint64_t ns1_size_blocks;
    uint32_t ns1_lba_bytes;
} nvme_info_t;

int  nvme_init(void);
const nvme_info_t* nvme_info(void);

int  nvme_admin_init(void);

int  nvme_io_init(void);
int  nvme_read (uint64_t lba, uint32_t count, void *buf);
int  nvme_write(uint64_t lba, uint32_t count, const void *buf);

#endif
