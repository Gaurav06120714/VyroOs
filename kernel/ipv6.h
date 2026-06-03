#ifndef IPV6_H
#define IPV6_H

#include "../include/types.h"

// Minimal IPv6 support: packet structures, ICMPv6 echo-request handler,
// link-local address derived from MAC (EUI-64). No NDP, no SLAAC routing,
// no fragmentation reassembly — those land in a follow-up phase.

#define ETHERTYPE_IPV6 0x86DD

typedef struct {
    uint8_t  ver_tc_fl[4];      // version<<28 | traffic_class<<20 | flow_label
    uint16_t payload_len;
    uint8_t  next_header;       // 58 = ICMPv6
    uint8_t  hop_limit;
    uint8_t  src[16];
    uint8_t  dst[16];
} __attribute__((packed)) ipv6_header_t;

typedef struct {
    uint8_t  type;              // 128 = echo request, 129 = echo reply
    uint8_t  code;
    uint16_t checksum;
} __attribute__((packed)) icmpv6_header_t;

#define ICMPV6_ECHO_REQUEST  128
#define ICMPV6_ECHO_REPLY    129

void ipv6_init(void);
const uint8_t* ipv6_local_addr(void);

// Feed a received Ethernet frame to IPv6. Returns 1 if consumed.
int  ipv6_input(const uint8_t* frame, uint16_t len);

#endif
