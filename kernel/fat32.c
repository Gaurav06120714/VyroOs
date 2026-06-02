#include "fat32.h"
#include "ata.h"

#define SECTOR_SIZE 512

static int      mounted = 0;
static uint16_t bytes_per_sector = 0;
static uint8_t  sectors_per_cluster = 0;
static uint16_t reserved_sectors = 0;
static uint8_t  num_fats = 0;
static uint32_t fat_size = 0;            // sectors per FAT
static uint32_t root_cluster = 0;
static uint32_t fat_start_lba = 0;
static uint32_t data_start_lba = 0;

static uint16_t le16(const uint8_t* p) { return p[0] | ((uint16_t)p[1] << 8); }
static uint32_t le32(const uint8_t* p) {
    return  (uint32_t)p[0]        | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

int fat32_is_mounted(void) { return mounted; }

int fat32_mount(void) {
    uint8_t bs[SECTOR_SIZE];
    if (!ata_read_sector(0, bs)) return 0;
    // BPB lives at offset 0 of LBA 0 for non-partitioned disks.
    // 0x55 0xAA signature at 510 / 511
    if (bs[510] != 0x55 || bs[511] != 0xAA) return 0;
    bytes_per_sector    = le16(bs + 0x0B);
    sectors_per_cluster = bs[0x0D];
    reserved_sectors    = le16(bs + 0x0E);
    num_fats            = bs[0x10];
    fat_size            = le32(bs + 0x24);     // BPB_FATSz32
    root_cluster        = le32(bs + 0x2C);     // BPB_RootClus
    if (bytes_per_sector != SECTOR_SIZE) return 0;
    if (sectors_per_cluster == 0) return 0;
    if (fat_size == 0) return 0;
    fat_start_lba  = reserved_sectors;
    data_start_lba = fat_start_lba + num_fats * fat_size;
    mounted = 1;
    return 1;
}

static uint32_t cluster_to_lba(uint32_t cluster) {
    return data_start_lba + (cluster - 2) * sectors_per_cluster;
}

static uint32_t fat_next(uint32_t cluster) {
    // FAT32 entry at FAT[cluster] is the next cluster; 0x0FFFFFFF means EOC.
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_lba    = fat_start_lba + fat_offset / SECTOR_SIZE;
    uint32_t off_in_sec = fat_offset % SECTOR_SIZE;
    uint8_t sec[SECTOR_SIZE];
    if (!ata_read_sector(fat_lba, sec)) return 0x0FFFFFFF;
    return le32(sec + off_in_sec) & 0x0FFFFFFF;
}

static void fmt83(const uint8_t* raw, char* out) {
    // raw[0..7] = name, raw[8..10] = ext. Trim trailing spaces, add dot.
    int p = 0;
    for (int i = 0; i < 8; i++) {
        if (raw[i] != ' ') out[p++] = (char)raw[i];
    }
    if (raw[8] != ' ') {
        out[p++] = '.';
        for (int i = 8; i < 11; i++) if (raw[i] != ' ') out[p++] = (char)raw[i];
    }
    out[p] = 0;
}

static int dir_walk(uint32_t cluster,
                    int (*cb)(const fat32_dirent_t* e, void* user),
                    void* user) {
    uint8_t sec[SECTOR_SIZE];
    while (cluster < 0x0FFFFFF8) {
        uint32_t lba = cluster_to_lba(cluster);
        for (uint8_t s = 0; s < sectors_per_cluster; s++) {
            if (!ata_read_sector(lba + s, sec)) return 0;
            for (int o = 0; o < SECTOR_SIZE; o += 32) {
                uint8_t first = sec[o];
                if (first == 0x00) return 1;                // end of dir
                if (first == 0xE5) continue;                // deleted
                if (sec[o + 11] == 0x0F) continue;          // LFN entry
                if (sec[o + 11] & 0x08) continue;           // volume label
                fat32_dirent_t e;
                fmt83(sec + o, e.name);
                e.attr = sec[o + 11];
                uint16_t hi = le16(sec + o + 20);
                uint16_t lo = le16(sec + o + 26);
                e.first_cluster = ((uint32_t)hi << 16) | lo;
                e.size = le32(sec + o + 28);
                int rc = cb(&e, user);
                if (rc == 0) return 1;                      // caller said stop
            }
        }
        cluster = fat_next(cluster);
    }
    return 1;
}

typedef struct { fat32_dirent_t* out; int max; int n; } list_ctx_t;
static int list_cb(const fat32_dirent_t* e, void* user) {
    list_ctx_t* c = (list_ctx_t*)user;
    if (c->n >= c->max) return 0;
    c->out[c->n++] = *e;
    return 1;
}

int fat32_list_root(fat32_dirent_t* out, int max) {
    if (!mounted) return 0;
    list_ctx_t c = { out, max, 0 };
    dir_walk(root_cluster, list_cb, &c);
    return c.n;
}

typedef struct { const char* target; fat32_dirent_t found; int ok; } find_ctx_t;
static int name_eq(const char* a, const char* b) {
    while (*a && *b) {
        char ca = *a, cb = *b;
        if (ca >= 'a' && ca <= 'z') ca -= 32;
        if (cb >= 'a' && cb <= 'z') cb -= 32;
        if (ca != cb) return 0;
        a++; b++;
    }
    return *a == 0 && *b == 0;
}
static int find_cb(const fat32_dirent_t* e, void* user) {
    find_ctx_t* c = (find_ctx_t*)user;
    if (name_eq(e->name, c->target)) {
        c->found = *e;
        c->ok = 1;
        return 0;
    }
    return 1;
}

int fat32_read_file(const char* name, uint8_t* out, uint32_t max_bytes) {
    if (!mounted) return -1;
    find_ctx_t c = { name, {0}, 0 };
    dir_walk(root_cluster, find_cb, &c);
    if (!c.ok) return -1;
    uint32_t cluster = c.found.first_cluster;
    uint32_t remaining = c.found.size;
    if (remaining > max_bytes) remaining = max_bytes;
    uint32_t written = 0;
    uint8_t sec[SECTOR_SIZE];
    while (cluster < 0x0FFFFFF8 && remaining > 0) {
        uint32_t lba = cluster_to_lba(cluster);
        for (uint8_t s = 0; s < sectors_per_cluster && remaining > 0; s++) {
            if (!ata_read_sector(lba + s, sec)) return (int)written;
            uint32_t take = remaining < SECTOR_SIZE ? remaining : SECTOR_SIZE;
            for (uint32_t i = 0; i < take; i++) out[written + i] = sec[i];
            written += take;
            remaining -= take;
        }
        cluster = fat_next(cluster);
    }
    return (int)written;
}
