#ifndef X25519_H
#define X25519_H

#include "../include/types.h"

void x25519(uint8_t out[32], const uint8_t scalar[32], const uint8_t u_in[32]);

void x25519_base(uint8_t out[32], const uint8_t scalar[32]);

int  x25519_selftest(void);

#endif
