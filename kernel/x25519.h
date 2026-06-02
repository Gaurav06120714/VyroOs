#ifndef X25519_H
#define X25519_H

#include "../include/types.h"

// X25519 per RFC 7748. Computes out = scalar * u-coord on Curve25519.
// scalar and u_in are both 32 bytes little-endian. out is 32 bytes.
void x25519(uint8_t out[32], const uint8_t scalar[32], const uint8_t u_in[32]);

// Convenience: out = scalar * basepoint (9). Used to derive a public key.
void x25519_base(uint8_t out[32], const uint8_t scalar[32]);

// Selftest: RFC 7748 §5.2 test vectors (two known answer tests + iterated test).
int  x25519_selftest(void);

#endif
