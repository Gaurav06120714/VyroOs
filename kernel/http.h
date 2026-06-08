#ifndef HTTP_H
#define HTTP_H

#include "../include/types.h"

int http_get(const uint8_t ip[4], uint16_t port,
             const char* host, const char* path,
             uint8_t* out, uint32_t out_max,
             uint32_t timeout_ms);

int http_parse_response(const uint8_t* buf, uint32_t len,
                        int* status,
                        const uint8_t** body_out, uint32_t* body_len_out,
                        int32_t* content_length);

#endif
