#ifndef CHACHA20_H
#define CHACHA20_H

#include "../include/types.h"

void chacha20_block(const uint8_t key[32], uint32_t counter,
                    const uint8_t nonce[12], uint8_t out[64]);

void chacha20_xor(const uint8_t key[32], uint32_t counter_start,
                  const uint8_t nonce[12],
                  const uint8_t* in, uint8_t* out, uint32_t len);

#endif
