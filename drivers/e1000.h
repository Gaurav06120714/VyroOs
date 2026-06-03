#ifndef E1000_H
#define E1000_H

#include "../include/types.h"

// Intel 8254x / 82573 / I217 / I218 / I219 family Ethernet detection.
// PCI vendor 0x8086; specific device IDs include 0x100E (82540EM, common in
// VirtualBox), 0x100F (82545EM), 0x10D3 (82574L), 0x153A (I217), 0x15B7
// (I219). QEMU also emulates 0x100E when launched with `-device e1000`.
//
// This scaffolding maps the controller and reads its MAC address from the
// Receive Address Low/High registers (RAL/RAH at MMIO 0x5400/0x5404).
// Full DMA ring TX/RX is the next phase and gives Vyro live Ethernet on
// real Intel-based hardware.

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
    uint32_t link_status;       // STATUS register
} e1000_info_t;

int  e1000_init(void);
const e1000_info_t* e1000_info(void);

#endif
