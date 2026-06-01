#ifndef SHA256_H
#define SHA256_H

#include "../include/types.h"

#define SHA256_DIGEST_SIZE 32

void sha256(const uint8_t* data, uint64_t len, uint8_t out[SHA256_DIGEST_SIZE]);
void sha256_hex(const uint8_t digest[SHA256_DIGEST_SIZE], char out_hex[65]);

#endif
