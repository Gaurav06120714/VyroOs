#ifndef RTL8139_H
#define RTL8139_H

#include "../include/types.h"

// RTL8139 NIC driver — real TX/RX so packets actually leave the VM.

#define RTL_VENDOR 0x10EC
#define RTL_DEVICE 0x8139

int  rtl8139_init();                                     // returns 1 on success
int  rtl8139_send(const uint8_t* frame, uint16_t len);   // send raw Ethernet frame
const uint8_t* rtl8139_mac();
uint64_t rtl8139_tx_count();
uint64_t rtl8139_rx_count();

// rx callback registered by the protocol layer; called from IRQ context.
typedef void (*rtl_rx_fn)(const uint8_t* frame, uint16_t len);
void rtl8139_set_rx_handler(rtl_rx_fn fn);

#endif
