#ifndef DHCP_H
#define DHCP_H

#include "../include/types.h"

#define DHCPDISCOVER 1
#define DHCPOFFER    2
#define DHCPREQUEST  3
#define DHCPACK      5

typedef struct {
    uint8_t  op;
    uint8_t  htype;
    uint8_t  hlen;
    uint8_t  hops;
    uint32_t xid;
    uint16_t secs;
    uint16_t flags;
    uint8_t  ciaddr[4];
    uint8_t  yiaddr[4];
    uint8_t  siaddr[4];
    uint8_t  giaddr[4];
    uint8_t  chaddr[16];
    uint8_t  sname[64];
    uint8_t  file[128];
    uint32_t magic;
} __attribute__((packed)) dhcp_packet_t;

void dhcp_init();
uint8_t dhcp_lease_active();
const uint8_t* dhcp_offered_ip();
const uint8_t* dhcp_gateway();
const uint8_t* dhcp_dns();

#endif
