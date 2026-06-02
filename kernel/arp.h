#ifndef ARP_H
#define ARP_H

#include "../include/types.h"

// Resolve dst_ip → MAC via ARP. Blocks polling RX up to timeout_ms.
// Returns 1 on success and fills out_mac. Maintains a small cache.
int  arp_resolve(const uint8_t dst_ip[4], uint32_t timeout_ms, uint8_t out_mac[6]);

// Feed an Ethernet frame to ARP. Returns 1 if it was an ARP packet (and consumed).
// Call from the RX dispatch path before handing the frame to IP.
int  arp_input(const uint8_t* frame, uint16_t len);

#endif
