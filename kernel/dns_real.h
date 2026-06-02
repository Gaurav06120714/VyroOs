#ifndef DNS_REAL_H
#define DNS_REAL_H

#include "../include/types.h"

// Sends a real DNS A query to dns_server_ip and waits up to timeout_ms for
// a response. On success returns 1 and fills out_ip with the resolved IPv4.
int dns_real_resolve(const char* hostname, const uint8_t dns_server_ip[4],
                     uint32_t timeout_ms, uint8_t out_ip[4]);

#endif
