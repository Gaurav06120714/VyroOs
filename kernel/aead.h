#ifndef AEAD_H
#define AEAD_H

#include "../include/types.h"

void aead_seal(const uint8_t key[32], const uint8_t nonce[12],
               const uint8_t* aad, uint32_t aad_len,
               const uint8_t* plain, uint32_t plain_len,
               uint8_t* cipher_out, uint8_t tag_out[16]);

int  aead_open(const uint8_t key[32], const uint8_t nonce[12],
               const uint8_t* aad, uint32_t aad_len,
               const uint8_t* cipher, uint32_t cipher_len,
               const uint8_t tag[16], uint8_t* plain_out);

int  aead_selftest(void);

#endif
