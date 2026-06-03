#ifndef XHCI_H
#define XHCI_H

#include "../include/types.h"

// xHCI USB 3.0 host-controller scaffolding.
// This phase detects the controller via PCI (class 0x0C, subclass 0x03,
// prog-if 0x30), parses the capability registers, and exposes the basic
// topology fields (number of slots, ports, interrupters, page size).
// Full device enumeration (command ring, slot allocation, port reset,
// USB descriptor read) is a separate future phase — the xHCI spec is
// hundreds of pages and warrants its own dedicated track.

typedef struct {
    int       present;
    uint64_t  mmio_base;
    uint8_t   caplength;
    uint16_t  hci_version;
    uint32_t  hcsparams1;     // device slots, interrupters, ports
    uint32_t  hcsparams2;
    uint32_t  hccparams1;
    uint32_t  page_size_bytes;
    uint32_t  max_slots;
    uint32_t  max_intrs;
    uint32_t  max_ports;
} xhci_info_t;

int  xhci_init(void);                 // returns 1 if controller found
const xhci_info_t* xhci_info(void);

// Halt and reset the controller. Returns 1 on success.
int  xhci_reset(void);

// Read the operational USBSTS register.
uint32_t xhci_status(void);

// Allocate DCBAA + Command Ring, program DCBAAP and CRCR, set CONFIG.MaxSlotsEn,
// and start the controller (USBCMD.RS=1). Caller must have called xhci_reset() first.
int  xhci_bring_up(void);

uint64_t xhci_dcbaa_phys(void);
uint64_t xhci_cmd_ring_phys(void);

// Event ring setup for Interrupter 0. Returns 1 on success.
int      xhci_event_ring_setup(void);
uint64_t xhci_event_ring_phys(void);
uint64_t xhci_erst_phys(void);

// Queue an Enable Slot command (TRB type 9) onto the command ring and ring
// the doorbell at DB[0] (host-controller doorbell). Doesn't wait for the
// event-ring completion; returns 1 if the queue + doorbell succeeded.
int      xhci_enable_slot(void);

uint32_t xhci_cmd_ring_index(void);

// Poll the event ring for the next event TRB. Returns 1 if an event was
// consumed (writes its TRB type, completion code and slot ID into the out
// parameters). Returns 0 if no event is available.
int  xhci_event_poll(uint8_t* trb_type, uint8_t* completion_code, uint8_t* slot_id);

#endif
