#ifndef NET_H
#define NET_H

#include "../include/types.h"

static inline uint16_t htons(uint16_t x) { return (uint16_t)((x << 8) | (x >> 8)); }
static inline uint16_t ntohs(uint16_t x) { return htons(x); }
static inline uint32_t htonl(uint32_t x) {
    return ((x & 0xFF) << 24) | ((x & 0xFF00) << 8) |
           ((x >> 8) & 0xFF00) | ((x >> 24) & 0xFF);
}

#define ETH_ALEN     6
#define ETHERTYPE_ARP  0x0806
#define ETHERTYPE_IPV4 0x0800

typedef struct {
    uint8_t  dst[ETH_ALEN];
    uint8_t  src[ETH_ALEN];
    uint16_t ethertype;
} __attribute__((packed)) eth_header_t;

typedef struct {
    uint16_t htype;
    uint16_t ptype;
    uint8_t  hlen;
    uint8_t  plen;
    uint16_t oper;
    uint8_t  sha[ETH_ALEN];
    uint8_t  spa[4];
    uint8_t  tha[ETH_ALEN];
    uint8_t  tpa[4];
} __attribute__((packed)) arp_packet_t;

typedef struct {
    uint8_t  ver_ihl;
    uint8_t  tos;
    uint16_t total_len;
    uint16_t id;
    uint16_t flags_frag;
    uint8_t  ttl;
    uint8_t  protocol;
    uint16_t checksum;
    uint8_t  src[4];
    uint8_t  dst[4];
} __attribute__((packed)) ipv4_header_t;

typedef struct {
    uint8_t  type;
    uint8_t  code;
    uint16_t checksum;
    uint16_t id;
    uint16_t seq;
} __attribute__((packed)) icmp_header_t;

#define IP_PROTO_ICMP 1
#define ICMP_ECHO_REQUEST 8

uint16_t net_checksum(const void* data, uint32_t len);

void net_init();
const uint8_t* net_mac();
const uint8_t* net_ip();
void net_set_ip(const uint8_t ip[4]);

#endif
