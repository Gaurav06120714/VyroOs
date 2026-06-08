#include "block.h"
#include "ahci.h"
#include "nvme.h"
#include "lba_xlate.h"

static block_device_t g_devs[BLOCK_MAX_DEVICES];
static int g_count = 0;

static int ahci_read_thunk(block_device_t *bd, uint64_t lba, uint32_t count, void *buf) {
    uint32_t port = (uint32_t)(uintptr_t)bd->transport;
    return ahci_port_read(port, lba, count, buf);
}

static int ahci_write_thunk(block_device_t *bd, uint64_t lba, uint32_t count, const void *buf) {
    uint32_t port = (uint32_t)(uintptr_t)bd->transport;
    return ahci_port_write(port, lba, count, buf);
}

static int nvme_read_thunk(block_device_t *bd, uint64_t lba, uint32_t count, void *buf) {
    (void)bd;
    return nvme_read(lba, count, buf);
}

static int nvme_write_thunk(block_device_t *bd, uint64_t lba, uint32_t count, const void *buf) {
    (void)bd;
    return nvme_write(lba, count, buf);
}

static void copy_str(char *dst, size_t cap, const char *src) {
    size_t i = 0;
    if (!src || cap == 0) { if (cap) dst[0] = '\0'; return; }
    while (src[i] && i + 1 < cap) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

static int register_ahci_port(uint32_t port) {
    if (g_count >= BLOCK_MAX_DEVICES) return 0;
    if (!ahci_port_init(port)) return 0;

    block_device_t *bd = &g_devs[g_count];
    bd->in_use             = 1;
    bd->kind               = BLOCK_KIND_AHCI;
    bd->index              = port;
    bd->logical_block_size = 512;
    bd->transport          = (void *)(uintptr_t)port;
    bd->read               = ahci_read_thunk;
    bd->write              = ahci_write_thunk;


    char model[48] = {0};
    uint64_t sectors = 0;
    if (ahci_port_identify(port, model, NULL, &sectors)) {
        bd->total_blocks = sectors;
        copy_str(bd->model, sizeof(bd->model), model[0] ? model : "SATA disk");
    } else {
        bd->total_blocks = 0;
        copy_str(bd->model, sizeof(bd->model), "SATA disk");
    }
    g_count++;
    return 1;
}

static int register_nvme_ns1(void) {
    if (g_count >= BLOCK_MAX_DEVICES) return 0;
    if (!nvme_admin_init()) return 0;
    if (!nvme_io_init())    return 0;

    const nvme_info_t *ni = nvme_info();
    block_device_t *bd = &g_devs[g_count];
    bd->in_use             = 1;
    bd->kind               = BLOCK_KIND_NVME;
    bd->index              = 1;
    bd->logical_block_size = ni->ns1_lba_bytes;
    bd->total_blocks       = ni->ns1_size_blocks;
    bd->transport          = NULL;
    bd->read               = nvme_read_thunk;
    bd->write              = nvme_write_thunk;
    copy_str(bd->model, sizeof(bd->model), ni->model);
    g_count++;
    return 1;
}

int block_probe(void) {
    g_count = 0;
    for (int i = 0; i < BLOCK_MAX_DEVICES; i++) g_devs[i].in_use = 0;


    if (ahci_init()) {
        uint32_t mask = ahci_active_ports();
        for (uint32_t p = 0; p < 32 && g_count < BLOCK_MAX_DEVICES; p++) {
            if (mask & (1u << p)) register_ahci_port(p);
        }
    }


    if (g_count < BLOCK_MAX_DEVICES && nvme_init()) {
        int before = g_count;
        if (register_nvme_ns1() && g_count > before) {

            uint32_t idx = (uint32_t)(g_count - 1);
            block_device_t *bd = block_get(idx);
            if (bd && bd->logical_block_size != 512) {
                lba_xlate_register(idx);
            }
        }
    }

    return g_count;
}

block_device_t* block_get(uint32_t idx) {
    if ((int)idx >= g_count || !g_devs[idx].in_use) return 0;
    return &g_devs[idx];
}

int block_count(void) { return g_count; }

int block_read(uint32_t idx, uint64_t lba, uint32_t count, void *buf) {
    block_device_t *bd = block_get(idx);
    if (!bd || !bd->read) return 0;
    return bd->read(bd, lba, count, buf);
}

int block_write(uint32_t idx, uint64_t lba, uint32_t count, const void *buf) {
    block_device_t *bd = block_get(idx);
    if (!bd || !bd->write) return 0;
    return bd->write(bd, lba, count, buf);
}

int block_register(block_device_t *src) {
    if (!src || g_count >= BLOCK_MAX_DEVICES) return -1;
    g_devs[g_count] = *src;
    return g_count++;
}
