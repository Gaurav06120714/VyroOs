#ifndef AEAD_H
#define AEAD_H

#include "../include/types.h"

// ChaCha20-Poly1305 AEAD per RFC 8439 §2.8.
// key: 32 bytes. nonce: 12 bytes (must be unique per key).

void aead_seal(const uint8_t key[32], const uint8_t nonce[12],
               const uint8_t* aad, uint32_t aad_len,
               const uint8_t* plain, uint32_t plain_len,
               uint8_t* cipher_out, uint8_t tag_out[16]);

// Returns 1 if MAC verifies (and writes plaintext); 0 on tag mismatch.
int  aead_open(const uint8_t key[32], const uint8_t nonce[12],
               const uint8_t* aad, uint32_t aad_len,
               const uint8_t* cipher, uint32_t cipher_len,
               const uint8_t tag[16], uint8_t* plain_out);

// Run RFC 8439 test vectors. Returns 1 on success.
int  aead_selftest(void);

#endif
