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

// Parse an HTTP/1.1 response. Sets *status (e.g. 200), points body to the
// first byte after the blank-line separator, and writes the parsed
// Content-Length into *content_length (-1 if absent or chunked). Returns 1
// on success, 0 on malformed input.
int http_parse_response(const uint8_t* buf, uint32_t len,
                        int* status,
                        const uint8_t** body_out, uint32_t* body_len_out,
                        int32_t* content_length);

#endif
