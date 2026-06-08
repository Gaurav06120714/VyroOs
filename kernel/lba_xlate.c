#include "lba_xlate.h"
#include "block.h"

#define FS_LBA_BYTES   512u
#define BOUNCE_BYTES  4096u

#define MAX_XLATE   4
static __attribute__((aligned(4096))) uint8_t g_bounce[MAX_XLATE][BOUNCE_BYTES];
static int       g_used[MAX_XLATE] = {0};
static uint32_t  g_owner[MAX_XLATE] = {0};
static uint32_t  g_native_bytes[MAX_XLATE] = {0};

static int slot_for(uint32_t idx) {
    for (int i = 0; i < MAX_XLATE; i++) if (g_used[i] && g_owner[i] == idx) return i;
    for (int i = 0; i < MAX_XLATE; i++) if (!g_used[i]) {
        block_device_t *bd = block_get(idx);
        if (!bd) return -1;
        g_used[i] = 1;
        g_owner[i] = idx;
        g_native_bytes[i] = bd->logical_block_size;
        return i;
    }
    return -1;
}

static int xlate_read(block_device_t *bd, uint64_t fs_lba, uint32_t count, void *buf) {
    uint32_t idx = (uint32_t)(uintptr_t)bd->transport;
    int slot = slot_for(idx);
    if (slot < 0) return 0;

    uint32_t ratio   = g_native_bytes[slot] / FS_LBA_BYTES;
    uint8_t *out     = (uint8_t *)buf;


    if ((fs_lba % ratio) == 0 && (count % ratio) == 0) {

        extern int (*lba_xlate_native_read )(block_device_t *bd, uint64_t lba, uint32_t count, void *buf);
        return lba_xlate_native_read(bd, fs_lba / ratio, count / ratio, out);
    }


    extern int (*lba_xlate_native_read )(block_device_t *bd, uint64_t lba, uint32_t count, void *buf);
    while (count) {
        uint64_t native_lba = fs_lba / ratio;
        uint32_t slice_idx  = fs_lba % ratio;
        if (!lba_xlate_native_read(bd, native_lba, 1, g_bounce[slot])) return 0;
        uint8_t *src = g_bounce[slot] + slice_idx * FS_LBA_BYTES;
        for (uint32_t b = 0; b < FS_LBA_BYTES; b++) out[b] = src[b];
        out    += FS_LBA_BYTES;
        fs_lba += 1;
        count  -= 1;
    }
    return 1;
}

static int xlate_write(block_device_t *bd, uint64_t fs_lba, uint32_t count, const void *buf) {
    uint32_t idx = (uint32_t)(uintptr_t)bd->transport;
    int slot = slot_for(idx);
    if (slot < 0) return 0;

    uint32_t ratio   = g_native_bytes[slot] / FS_LBA_BYTES;
    const uint8_t *in = (const uint8_t *)buf;

    extern int (*lba_xlate_native_read )(block_device_t *bd, uint64_t lba, uint32_t count, void *buf);
    extern int (*lba_xlate_native_write)(block_device_t *bd, uint64_t lba, uint32_t count, const void *buf);

    if ((fs_lba % ratio) == 0 && (count % ratio) == 0) {
        return lba_xlate_native_write(bd, fs_lba / ratio, count / ratio, in);
    }


    while (count) {
        uint64_t native_lba = fs_lba / ratio;
        uint32_t slice_idx  = fs_lba % ratio;
        if (!lba_xlate_native_read(bd, native_lba, 1, g_bounce[slot])) return 0;
        uint8_t *dst = g_bounce[slot] + slice_idx * FS_LBA_BYTES;
        for (uint32_t b = 0; b < FS_LBA_BYTES; b++) dst[b] = in[b];
        if (!lba_xlate_native_write(bd, native_lba, 1, g_bounce[slot])) return 0;
        in     += FS_LBA_BYTES;
        fs_lba += 1;
        count  -= 1;
    }
    return 1;
}

int (*lba_xlate_native_read )(block_device_t *bd, uint64_t lba, uint32_t count, void *buf) = 0;
int (*lba_xlate_native_write)(block_device_t *bd, uint64_t lba, uint32_t count, const void *buf) = 0;

int lba_xlate_register(uint32_t block_idx) {
    block_device_t *bd = block_get(block_idx);
    if (!bd) return 0;
    if (bd->logical_block_size == FS_LBA_BYTES) return 1;
    if (bd->logical_block_size != BOUNCE_BYTES) return 0;

    if (slot_for(block_idx) < 0) return 0;


    lba_xlate_native_read  = bd->read;
    lba_xlate_native_write = bd->write;
    bd->read  = xlate_read;
    bd->write = xlate_write;


    bd->total_blocks       = bd->total_blocks * (BOUNCE_BYTES / FS_LBA_BYTES);
    bd->logical_block_size = FS_LBA_BYTES;
    return 1;
}
