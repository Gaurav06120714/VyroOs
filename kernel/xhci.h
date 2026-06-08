#ifndef XHCI_H
#define XHCI_H

#include "../include/types.h"

typedef struct {
    int       present;
    uint64_t  mmio_base;
    uint8_t   caplength;
    uint16_t  hci_version;
    uint32_t  hcsparams1;
    uint32_t  hcsparams2;
    uint32_t  hccparams1;
    uint32_t  page_size_bytes;
    uint32_t  max_slots;
    uint32_t  max_intrs;
    uint32_t  max_ports;
} xhci_info_t;

int  xhci_init(void);
const xhci_info_t* xhci_info(void);

int  xhci_reset(void);

uint32_t xhci_status(void);

int  xhci_bring_up(void);

uint64_t xhci_dcbaa_phys(void);
uint64_t xhci_cmd_ring_phys(void);

int      xhci_event_ring_setup(void);
uint64_t xhci_event_ring_phys(void);
uint64_t xhci_erst_phys(void);

int      xhci_enable_slot(void);

uint32_t xhci_cmd_ring_index(void);

int  xhci_event_poll(uint8_t* trb_type, uint8_t* completion_code, uint8_t* slot_id);

#endif
