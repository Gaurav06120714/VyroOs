#ifndef NVME_H
#define NVME_H

#include "../include/types.h"

// NVMe controller. v5.7 detected via PCI and parsed CAP. vC.6.2 brings
// up the Admin Submission/Completion Queues and issues Identify Controller
// + Identify Namespace 1 so we know the device's real serial, model, and
// LBA size. I/O queues + read/write land in vC.6.3.

typedef struct {
    int      present;
    uint64_t mmio_base;
    uint64_t cap;
    uint32_t version;
    uint32_t max_q_entries;
    uint8_t  doorbell_stride;

    // vC.6.2 additions
    int      admin_ready;
    char     serial[21];          // 20 ASCII + NUL
    char     model[41];           // 40 ASCII + NUL
    uint64_t ns1_size_blocks;     // namespace 1 size in LBAs
    uint32_t ns1_lba_bytes;       // typically 512 or 4096
} nvme_info_t;

int  nvme_init(void);
const nvme_info_t* nvme_info(void);

// vC.6.2: bring up admin queues + identify. Returns 1 on success.
int  nvme_admin_init(void);

// vC.6.3: create I/O Submission Queue 1 + Completion Queue 1, then issue
// NVMe READ (0x02) / WRITE (0x01) commands on namespace 1.
// Buffer must be physically contiguous and aligned to ns1_lba_bytes.
int  nvme_io_init(void);
int  nvme_read (uint64_t lba, uint32_t count, void *buf);
int  nvme_write(uint64_t lba, uint32_t count, const void *buf);

#endif
