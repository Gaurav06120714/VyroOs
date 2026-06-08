#ifndef IPV6_H
#define IPV6_H

#include "../include/types.h"

#define ETHERTYPE_IPV6 0x86DD

typedef struct {
    uint8_t  ver_tc_fl[4];
    uint16_t payload_len;
    uint8_t  next_header;
    uint8_t  hop_limit;
    uint8_t  src[16];
    uint8_t  dst[16];
} __attribute__((packed)) ipv6_header_t;

typedef struct {
    uint8_t  type;
    uint8_t  code;
    uint16_t checksum;
} __attribute__((packed)) icmpv6_header_t;

#define ICMPV6_ECHO_REQUEST  128
#define ICMPV6_ECHO_REPLY    129

void ipv6_init(void);
const uint8_t* ipv6_local_addr(void);

int  ipv6_input(const uint8_t* frame, uint16_t len);

#endif
