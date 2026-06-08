#include "fat32.h"
#include "ata.h"
#include "block.h"

#define SECTOR_SIZE 512

static int      g_use_block  = 0;
static uint32_t g_block_idx  = 0;

static int      mounted = 0;
static uint16_t bytes_per_sector = 0;
static uint8_t  sectors_per_cluster = 0;
static uint16_t reserved_sectors = 0;
static uint8_t  num_fats = 0;
static uint32_t fat_size = 0;
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


    if (bs[510] != 0x55 || bs[511] != 0xAA) return 0;
    bytes_per_sector    = le16(bs + 0x0B);
    sectors_per_cluster = bs[0x0D];
    reserved_sectors    = le16(bs + 0x0E);
    num_fats            = bs[0x10];
    fat_size            = le32(bs + 0x24);
    root_cluster        = le32(bs + 0x2C);
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

    uint32_t fat_offset = cluster * 4;
    uint32_t fat_lba    = fat_start_lba + fat_offset / SECTOR_SIZE;
    uint32_t off_in_sec = fat_offset % SECTOR_SIZE;
    uint8_t sec[SECTOR_SIZE];
    if (!ata_read_sector(fat_lba, sec)) return 0x0FFFFFFF;
    return le32(sec + off_in_sec) & 0x0FFFFFFF;
}

static void fmt83(const uint8_t* raw, char* out) {

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
                if (first == 0x00) return 1;
                if (first == 0xE5) continue;
                if (sec[o + 11] == 0x0F) continue;
                if (sec[o + 11] & 0x08) continue;
                fat32_dirent_t e;
                fmt83(sec + o, e.name);
                e.attr = sec[o + 11];
                uint16_t hi = le16(sec + o + 20);
                uint16_t lo = le16(sec + o + 26);
                e.first_cluster = ((uint32_t)hi << 16) | lo;
                e.size = le32(sec + o + 28);
                int rc = cb(&e, user);
                if (rc == 0) return 1;
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

int fat32_list_cluster(uint32_t cluster, fat32_dirent_t* out, int max) {
    if (!mounted) return 0;
    list_ctx_t c = { out, max, 0 };
    dir_walk(cluster, list_cb, &c);
    return c.n;
}

int fat32_path_lookup(const char* path, fat32_dirent_t* out_e) {
    if (!mounted || !path) return 0;
    uint32_t cluster = root_cluster;
    if (*path == '/') path++;
    if (*path == 0) {

        for (int i = 0; i < 12; i++) out_e->name[i] = 0;
        out_e->name[0] = '/';
        out_e->attr = 0x10;
        out_e->first_cluster = root_cluster;
        out_e->size = 0;
        return 1;
    }
    while (1) {

        char comp[13]; int cp = 0;
        while (*path && *path != '/' && cp < 12) comp[cp++] = *path++;
        comp[cp] = 0;
        if (cp == 0) return 0;

        find_ctx_t fc = { comp, {0}, 0 };
        dir_walk(cluster, find_cb, &fc);
        if (!fc.ok) return 0;
        if (*path == 0) {
            *out_e = fc.found;
            return 1;
        }

        if (!(fc.found.attr & 0x10)) return 0;
        cluster = fc.found.first_cluster;
        if (cluster == 0) cluster = root_cluster;
        if (*path == '/') path++;
    }
}

static void fat_write_entry(uint32_t cluster, uint32_t value) {
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_lba    = fat_start_lba + fat_offset / SECTOR_SIZE;
    uint32_t off_in_sec = fat_offset % SECTOR_SIZE;
    uint8_t sec[SECTOR_SIZE];
    if (!ata_read_sector(fat_lba, sec)) return;
    uint32_t old = le32(sec + off_in_sec) & 0xF0000000;
    uint32_t v   = (value & 0x0FFFFFFF) | old;
    sec[off_in_sec + 0] = v & 0xFF;
    sec[off_in_sec + 1] = (v >> 8) & 0xFF;
    sec[off_in_sec + 2] = (v >> 16) & 0xFF;
    sec[off_in_sec + 3] = (v >> 24) & 0xFF;

    for (uint32_t f = 0; f < num_fats; f++) {
        ata_write_sector(fat_lba + f * fat_size, sec);
    }
}

static uint32_t alloc_cluster(void) {

    uint8_t sec[SECTOR_SIZE];
    uint32_t entries_per_sec = SECTOR_SIZE / 4;
    for (uint32_t i = 0; i < fat_size; i++) {
        if (!ata_read_sector(fat_start_lba + i, sec)) return 0;
        for (uint32_t j = 0; j < entries_per_sec; j++) {
            uint32_t cl = i * entries_per_sec + j;
            if (cl < 2) continue;
            uint32_t v = le32(sec + j * 4) & 0x0FFFFFFF;
            if (v == 0) {
                fat_write_entry(cl, 0x0FFFFFFF);
                return cl;
            }
        }
    }
    return 0;
}

static int fmt83_raw(const char* name, uint8_t out[11]) {
    for (int i = 0; i < 11; i++) out[i] = ' ';
    int p = 0;
    for (; *name && *name != '.' && p < 8; name++, p++) {
        char c = *name;
        if (c >= 'a' && c <= 'z') c -= 32;
        out[p] = (uint8_t)c;
    }
    if (*name == '.') name++;
    p = 8;
    for (; *name && p < 11; name++, p++) {
        char c = *name;
        if (c >= 'a' && c <= 'z') c -= 32;
        out[p] = (uint8_t)c;
    }
    return 1;
}

static int find_or_alloc_dirslot(const char* name, uint32_t* out_lba,
                                  uint32_t* out_off, int create) {
    uint8_t raw[11];
    fmt83_raw(name, raw);
    uint32_t cluster = root_cluster;
    uint8_t sec[SECTOR_SIZE];
    while (cluster < 0x0FFFFFF8) {
        uint32_t lba = cluster_to_lba(cluster);
        for (uint8_t s = 0; s < sectors_per_cluster; s++) {
            if (!ata_read_sector(lba + s, sec)) return 0;
            for (int o = 0; o < SECTOR_SIZE; o += 32) {
                uint8_t first = sec[o];
                if (first == 0x00 || first == 0xE5) {
                    if (create) {
                        *out_lba = lba + s; *out_off = (uint32_t)o;
                        return 1;
                    }
                    if (first == 0x00) return 0;
                    continue;
                }
                if (sec[o + 11] == 0x0F) continue;
                int match = 1;
                for (int k = 0; k < 11; k++) if (sec[o + k] != raw[k]) { match = 0; break; }
                if (match) {
                    *out_lba = lba + s; *out_off = (uint32_t)o;
                    return 1;
                }
            }
        }
        cluster = fat_next(cluster);
    }
    return 0;
}

int fat32_write_file(const char* name, const uint8_t* data, uint32_t len) {
    if (!mounted) return -1;
    uint8_t raw[11];
    fmt83_raw(name, raw);


    uint32_t slot_lba, slot_off;
    int existing = find_or_alloc_dirslot(name, &slot_lba, &slot_off, 0);
    int created  = 0;
    if (!existing) {
        if (!find_or_alloc_dirslot(name, &slot_lba, &slot_off, 1)) return -1;
        created = 1;
    }


    uint32_t cluster_size = (uint32_t)sectors_per_cluster * SECTOR_SIZE;
    uint32_t clusters_needed = (len + cluster_size - 1) / cluster_size;
    if (clusters_needed == 0) clusters_needed = 1;

    uint32_t first_cl = 0, prev_cl = 0;
    for (uint32_t i = 0; i < clusters_needed; i++) {
        uint32_t cl = alloc_cluster();
        if (cl == 0) {


            uint32_t c = first_cl;
            while (c >= 2 && c < 0x0FFFFFF8) {
                uint32_t next = fat_next(c);
                fat_write_entry(c, 0);
                if (c == prev_cl) break;
                c = next;
            }
            return -1;
        }
        if (first_cl == 0) first_cl = cl;
        if (prev_cl)  fat_write_entry(prev_cl, cl);
        prev_cl = cl;
    }

    fat_write_entry(prev_cl, 0x0FFFFFFF);


    uint32_t off = 0;
    uint32_t cl = first_cl;
    uint8_t sec[SECTOR_SIZE];
    while (cl < 0x0FFFFFF8 && off < len) {
        uint32_t lba = cluster_to_lba(cl);
        for (uint8_t s = 0; s < sectors_per_cluster && off < len; s++) {
            for (int i = 0; i < SECTOR_SIZE; i++) sec[i] = 0;
            uint32_t take = len - off;
            if (take > SECTOR_SIZE) take = SECTOR_SIZE;
            for (uint32_t i = 0; i < take; i++) sec[i] = data[off + i];
            if (!ata_write_sector(lba + s, sec)) return -1;
            off += take;
        }
        cl = fat_next(cl);
    }


    if (!ata_read_sector(slot_lba, sec)) return -1;
    uint8_t* d = sec + slot_off;
    for (int i = 0; i < 11; i++) d[i] = raw[i];
    d[11] = 0x20;
    for (int i = 12; i < 20; i++) d[i] = 0;
    d[20] = (uint8_t)(first_cl >> 16);
    d[21] = (uint8_t)(first_cl >> 24);
    for (int i = 22; i < 26; i++) d[i] = 0;
    d[26] = (uint8_t)(first_cl);
    d[27] = (uint8_t)(first_cl >> 8);
    d[28] = (uint8_t)(len);
    d[29] = (uint8_t)(len >> 8);
    d[30] = (uint8_t)(len >> 16);
    d[31] = (uint8_t)(len >> 24);
    if (!ata_write_sector(slot_lba, sec)) return -1;
    (void)created;
    return (int)len;
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

void fat32_use_block(int yes, uint32_t block_idx) {
    g_use_block = yes ? 1 : 0;
    g_block_idx = block_idx;
}

int fat32_mount_block(uint32_t block_idx) {

    block_device_t *bd = block_get(block_idx);
    if (!bd) return 0;
    if (bd->logical_block_size != SECTOR_SIZE) return 0;

    uint8_t bs[SECTOR_SIZE];
    if (!block_read(block_idx, 0, 1, bs)) return 0;
    if (bs[510] != 0x55 || bs[511] != 0xAA) return 0;

    fat32_use_block(1, block_idx);
    int ok = fat32_mount();
    if (!ok) fat32_use_block(0, 0);
    return ok;
}
