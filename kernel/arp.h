#ifndef ARP_H
#define ARP_H

#include "../include/types.h"

int  arp_resolve(const uint8_t dst_ip[4], uint32_t timeout_ms, uint8_t out_mac[6]);

int  arp_input(const uint8_t* frame, uint16_t len);

#endif
