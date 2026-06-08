#ifndef DNS_REAL_H
#define DNS_REAL_H

#include "../include/types.h"

int dns_real_resolve(const char* hostname, const uint8_t dns_server_ip[4],
                     uint32_t timeout_ms, uint8_t out_ip[4]);

#endif
