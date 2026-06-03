#ifndef RSA_PSS_H
#define RSA_PSS_H

#include "../include/types.h"

// RSA-PSS signature verification with SHA-256, salt length = 32, MGF1-SHA-256
// (the parameters TLS 1.3's rsa_pss_rsae_sha256 mandates). All buffers are
// big-endian. Returns 1 if signature verifies, 0 otherwise.

int rsa_pss_sha256_verify(const uint8_t* n, uint32_t n_len,
                          const uint8_t* e, uint32_t e_len,
                          const uint8_t* sig, uint32_t sig_len,
                          const uint8_t* msg, uint32_t msg_len);

#endif
