#ifndef BIGNUM_4K_H
#define BIGNUM_4K_H

#include "../include/types.h"

#define BN4K_LIMBS  128
#define BN4K_BITS   (BN4K_LIMBS * 32)
#define BN4K_BYTES  (BN4K_LIMBS * 4)

typedef uint32_t bn4k_t[BN4K_LIMBS];

void bn4k_from_be(bn4k_t r, const uint8_t* be, uint32_t len);
void bn4k_to_be(uint8_t* out, uint32_t out_len, const bn4k_t a);
void bn4k_modexp(bn4k_t r, const bn4k_t base, const bn4k_t exp, const bn4k_t m);
int  bn4k_cmp(const bn4k_t a, const bn4k_t b);
int  bn4k_is_zero(const bn4k_t a);

int rsa4k_pkcs1_v15_sha256_verify(const uint8_t* n, uint32_t n_len,
                                  const uint8_t* e, uint32_t e_len,
                                  const uint8_t* sig, uint32_t sig_len,
                                  const uint8_t* msg, uint32_t msg_len);

#endif
