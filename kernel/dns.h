#ifndef DNS_H
#define DNS_H

#include "../include/types.h"

// DNS header
typedef struct {
    uint16_t id;
    uint16_t flags;
    uint16_t qd_count;
    uint16_t an_count;
    uint16_t ns_count;
    uint16_t ar_count;
} __attribute__((packed)) dns_header_t;

#define DNS_TYPE_A    1
#define DNS_CLASS_IN  1

int dns_resolve(const char* hostname, uint8_t ip_out[4]);

#endif
