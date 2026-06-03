#ifndef ECDSA_H
#define ECDSA_H

#include "../include/types.h"

// ECDSA verification on NIST P-256. SHA-256 message hash.
// Public key is the uncompressed affine (X, Y) — 32 bytes each, big-endian.
// Signature is DER-encoded SEQUENCE { INTEGER r, INTEGER s } (as it appears
// in X.509 / TLS).
//
// Returns 1 if the signature verifies, 0 otherwise.

int ecdsa_p256_sha256_verify(const uint8_t pubkey_x[32], const uint8_t pubkey_y[32],
                             const uint8_t* sig_der, uint32_t sig_der_len,
                             const uint8_t* msg, uint32_t msg_len);

// Selftest: verify a known RFC 6979 / NIST KAT against the implementation.
int ecdsa_selftest(void);

#endif
