#ifndef GDT_H
#define GDT_H

#include "../include/types.h"

// Segment selectors (index << 3 | RPL)
#define SEL_KCODE  0x08          // Ring 0 code
#define SEL_KDATA  0x10          // Ring 0 data
#define SEL_UCODE  (0x18 | 3)    // Ring 3 code = 0x1B
#define SEL_UDATA  (0x20 | 3)    // Ring 3 data = 0x23
#define SEL_TSS    0x28          // TSS descriptor

void gdt_init();
void tss_set_rsp0(uint64_t rsp0);

#endif
