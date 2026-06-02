#ifndef RSA_H
#define RSA_H

#include "../include/types.h"

// RSA-PKCS1-v1_5 signature verification with SHA-256.
// All buffers are big-endian byte arrays as they appear in DER.
// n, e: public key (modulus, exponent). n_len up to 256 bytes (RSA-2048).
// sig: signature, must equal n_len.
// msg: the data that was signed. We compute SHA-256(msg) and compare to the
// recovered digest from the unpadded signature.
//
// Returns 1 if the signature verifies.

int rsa_pkcs1_v15_sha256_verify(const uint8_t* n, uint32_t n_len,
                                const uint8_t* e, uint32_t e_len,
                                const uint8_t* sig, uint32_t sig_len,
                                const uint8_t* msg, uint32_t msg_len);

// Selftest: builds a small known-answer case and exercises the modexp path.
int rsa_selftest(void);

#endif
