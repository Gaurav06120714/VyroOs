#ifndef POLY1305_H
#define POLY1305_H

#include "../include/types.h"

void poly1305_mac(uint8_t out[16], const uint8_t* msg, uint32_t len,
                  const uint8_t key[32]);

#endif
