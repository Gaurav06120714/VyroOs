#ifndef BIGNUM_H
#define BIGNUM_H

#include "../include/types.h"

// Fixed-width bignum: 64 × 32-bit limbs = 2048 bits.
// Little-endian limb order (a[0] is least significant 32 bits).
// All operations are NOT constant-time — adequate for verification
// (public-key operations only); never use this for private-key signing.

#define BN_LIMBS  64
#define BN_BITS   (BN_LIMBS * 32)
#define BN_BYTES  (BN_LIMBS * 4)

typedef uint32_t bn_t[BN_LIMBS];

void bn_zero(bn_t r);
void bn_copy(bn_t r, const bn_t a);
int  bn_cmp(const bn_t a, const bn_t b);                  // -1, 0, +1
int  bn_is_zero(const bn_t a);

// Big-endian byte I/O. `len` is the actual number of bytes (≤ BN_BYTES).
void bn_from_be(bn_t r, const uint8_t* be, uint32_t len);
void bn_to_be(uint8_t* out, uint32_t out_len, const bn_t a);

// r = (a + b) mod m, assuming a < m and b < m.
void bn_addmod(bn_t r, const bn_t a, const bn_t b, const bn_t m);

// r = (a * b) mod m. Robust against overlap (r may alias a or b).
void bn_mulmod(bn_t r, const bn_t a, const bn_t b, const bn_t m);

// r = base^exp mod m. exp is an arbitrary 2048-bit big-endian-loaded bignum.
void bn_modexp(bn_t r, const bn_t base, const bn_t exp, const bn_t m);

#endif
