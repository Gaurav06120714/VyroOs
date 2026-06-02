#ifndef HTTP_H
#define HTTP_H

#include "../include/types.h"

// Minimal HTTP/1.1 client. Plaintext only this phase; HTTPS requires the full
// TLS handshake completion (client Finished + application traffic keys) which
// is its own follow-up phase.

// Issue a GET against host:port and read the response into `out`.
// Returns bytes read on success, < 0 on failure.
int http_get(const uint8_t ip[4], uint16_t port,
             const char* host, const char* path,
             uint8_t* out, uint32_t out_max,
             uint32_t timeout_ms);

#endif
