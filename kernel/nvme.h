#ifndef NVME_H
#define NVME_H

#include "../include/types.h"

// NVMe controller detection. Identifies a controller via PCI class 0x01,
// subclass 0x08, prog-if 0x02, reads BAR0 MMIO and parses the Controller
// Capability register (CAP, 8 bytes at offset 0). Full Admin Queue setup
// + Identify Controller + namespace I/O is the next phase — that's what
// boots from a real NVMe SSD on modern PCs and Macs.

typedef struct {
    int      present;
    uint64_t mmio_base;
    uint64_t cap;            // raw CAP register
    uint32_t version;        // VS register
    uint32_t max_q_entries;  // CAP.MQES + 1
    uint8_t  doorbell_stride;// CAP.DSTRD
} nvme_info_t;

int  nvme_init(void);
const nvme_info_t* nvme_info(void);

#endif
