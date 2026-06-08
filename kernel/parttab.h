#ifndef PARTTAB_H
#define PARTTAB_H

#include "../include/types.h"

typedef struct {
    int       in_use;
    uint8_t   type_id;
    uint64_t  start_lba;
    uint64_t  length_lba;
    char      name[36];
} parttab_entry_t;

#define PARTTAB_MAX  8

typedef struct {
    int               initialized;
    int               source_gpt;
    uint32_t          count;
    parttab_entry_t   entries[PARTTAB_MAX];
} parttab_t;

int parttab_probe(uint32_t block_idx, parttab_t *out);

int parttab_register_partition(uint32_t parent_idx, uint64_t start_lba,
                               uint64_t length_lba, const char *label);

#endif
