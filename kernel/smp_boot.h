#ifndef SMP_BOOT_H
#define SMP_BOOT_H

#include "../include/types.h"

#define SMP_TRAMP_PHYS  0x8000
#define SMP_FLAG_PHYS   0x9000

void smp_start_aps(void);
uint32_t smp_ap_count(void);

uint32_t smp_ap_in_c_count(void);

const uint8_t* smp_ap_in_c_map(void);

#endif
