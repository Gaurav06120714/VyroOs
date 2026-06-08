#ifndef BIGNUM_H
#define BIGNUM_H

#include "../include/types.h"

#define BN_LIMBS  64
#define BN_BITS   (BN_LIMBS * 32)
#define BN_BYTES  (BN_LIMBS * 4)

typedef uint32_t bn_t[BN_LIMBS];

void bn_zero(bn_t r);
void bn_copy(bn_t r, const bn_t a);
int  bn_cmp(const bn_t a, const bn_t b);
int  bn_is_zero(const bn_t a);

void bn_from_be(bn_t r, const uint8_t* be, uint32_t len);
void bn_to_be(uint8_t* out, uint32_t out_len, const bn_t a);

void bn_addmod(bn_t r, const bn_t a, const bn_t b, const bn_t m);

void bn_mulmod(bn_t r, const bn_t a, const bn_t b, const bn_t m);

void bn_modexp(bn_t r, const bn_t base, const bn_t exp, const bn_t m);

#endif
