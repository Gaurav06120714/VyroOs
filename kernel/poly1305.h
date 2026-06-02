#ifndef POLY1305_H
#define POLY1305_H

#include "../include/types.h"

// Poly1305 one-shot MAC per RFC 8439 §2.5.
// key: 32 bytes (16 r + 16 s). out: 16-byte authentication tag.

void poly1305_mac(uint8_t out[16], const uint8_t* msg, uint32_t len,
                  const uint8_t key[32]);

#endif
