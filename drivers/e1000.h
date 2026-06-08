#ifndef E1000_H
#define E1000_H

#include "../include/types.h"

#define E1000_REG_CTRL  0x0000
#define E1000_REG_STATUS 0x0008
#define E1000_REG_EERD  0x0014
#define E1000_REG_RAL   0x5400
#define E1000_REG_RAH   0x5404

typedef struct {
    int      present;
    uint16_t device_id;
    uint64_t mmio_base;
    uint8_t  mac[6];
    uint32_t link_status;
} e1000_info_t;

int  e1000_init(void);
const e1000_info_t* e1000_info(void);

#endif
