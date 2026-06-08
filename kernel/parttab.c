#include "parttab.h"
#include "block.h"

#define SECTOR  512u

static int read_sector(uint32_t idx, uint64_t lba, uint8_t *buf) {
    return block_read(idx, lba, 1, buf);
}

static uint16_t le16(const uint8_t *p) { return p[0] | ((uint16_t)p[1] << 8); }
static uint32_t le32(const uint8_t *p) {
    return  (uint32_t)p[0]        | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint64_t le64(const uint8_t *p) {
    return  (uint64_t)le32(p) | ((uint64_t)le32(p + 4) << 32);
}

static int parse_mbr(uint32_t idx, parttab_t *out) {
    uint8_t bs[SECTOR];
    if (!read_sector(idx, 0, bs)) return 0;
    if (bs[510] != 0x55 || bs[511] != 0xAA) return 0;

    out->source_gpt = 0;
    out->count = 0;
    for (int i = 0; i < 4; i++) {
        const uint8_t *e = bs + 0x1BE + i * 16;
        uint8_t  type   = e[4];
        uint32_t start  = le32(e + 8);
        uint32_t count  = le32(e + 12);
        if (type == 0 || count == 0) continue;
        if (out->count >= PARTTAB_MAX) break;
        parttab_entry_t *p = &out->entries[out->count++];
        p->in_use     = 1;
        p->type_id    = type;
        p->start_lba  = start;
        p->length_lba = count;
        p->name[0]    = 'p'; p->name[1] = '1' + i; p->name[2] = '\0';
    }
    out->initialized = 1;
    return 1;
}

static int parse_gpt(uint32_t idx, parttab_t *out) {
    uint8_t hdr[SECTOR];
    if (!read_sector(idx, 1, hdr)) return 0;

    static const uint8_t SIG[8] = {'E','F','I',' ','P','A','R','T'};
    for (int i = 0; i < 8; i++) if (hdr[i] != SIG[i]) return 0;

    uint64_t pe_lba    = le64(hdr + 72);
    uint32_t pe_count  = le32(hdr + 80);
    uint32_t pe_size   = le32(hdr + 84);
    if (pe_size == 0 || pe_size > SECTOR) return 0;
    if (pe_count > 128) pe_count = 128;

    uint32_t per_sector = SECTOR / pe_size;
    uint8_t  sec[SECTOR];

    out->source_gpt = 1;
    out->count = 0;

    for (uint32_t i = 0; i < pe_count; i++) {
        if ((i % per_sector) == 0) {
            if (!read_sector(idx, pe_lba + (i / per_sector), sec)) return 0;
        }
        const uint8_t *e = sec + (i % per_sector) * pe_size;


        int empty = 1;
        for (int b = 0; b < 16; b++) if (e[b]) { empty = 0; break; }
        if (empty) continue;

        uint64_t first_lba = le64(e + 32);
        uint64_t last_lba  = le64(e + 40);
        if (last_lba < first_lba) continue;

        if (out->count >= PARTTAB_MAX) break;
        parttab_entry_t *p = &out->entries[out->count++];
        p->in_use     = 1;
        p->type_id    = e[0];
        p->start_lba  = first_lba;
        p->length_lba = last_lba - first_lba + 1;


        for (int c = 0; c < 35; c++) {
            uint8_t lo = e[56 + c*2];
            p->name[c] = (lo >= 0x20 && lo <= 0x7E) ? (char)lo : '\0';
            if (!p->name[c]) break;
        }
        p->name[35] = '\0';
    }
    out->initialized = 1;
    return 1;
}

int parttab_probe(uint32_t idx, parttab_t *out) {
    if (!out) return 0;
    for (uint32_t i = 0; i < sizeof(*out); i++) ((uint8_t *)out)[i] = 0;


    if (parse_gpt(idx, out)) return 1;
    if (parse_mbr(idx, out)) return 1;
    return 0;
}

typedef struct {
    uint32_t parent;
    uint64_t start;
    uint64_t length;
} window_t;

#define MAX_WINDOWS PARTTAB_MAX
static window_t g_windows[MAX_WINDOWS];
static int      g_wcount = 0;

static int win_read(block_device_t *bd, uint64_t lba, uint32_t count, void *buf) {
    window_t *w = &g_windows[(uint32_t)(uintptr_t)bd->transport];
    if (lba + count > w->length) return 0;
    return block_read(w->parent, w->start + lba, count, buf);
}
static int win_write(block_device_t *bd, uint64_t lba, uint32_t count, const void *buf) {
    window_t *w = &g_windows[(uint32_t)(uintptr_t)bd->transport];
    if (lba + count > w->length) return 0;
    return block_write(w->parent, w->start + lba, count, buf);
}

extern int block_register(block_device_t *bd);

int parttab_register_partition(uint32_t parent_idx, uint64_t start_lba,
                               uint64_t length_lba, const char *label) {
    if (g_wcount >= MAX_WINDOWS) return -1;

    block_device_t *parent = block_get(parent_idx);
    if (!parent) return -1;

    int wi = g_wcount++;
    g_windows[wi].parent = parent_idx;
    g_windows[wi].start  = start_lba;
    g_windows[wi].length = length_lba;

    block_device_t bd = {0};
    bd.in_use             = 1;
    bd.kind               = parent->kind;
    bd.index              = parent->index;
    bd.logical_block_size = 512;
    bd.total_blocks       = length_lba;
    bd.transport          = (void *)(uintptr_t)wi;
    bd.read               = win_read;
    bd.write              = win_write;
    if (label) {
        for (int i = 0; i < (int)sizeof(bd.model) - 1 && label[i]; i++) bd.model[i] = label[i];
    }

    return block_register(&bd);
}
