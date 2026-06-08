#ifndef ECDSA_H
#define ECDSA_H

#include "../include/types.h"

int ecdsa_p256_sha256_verify(const uint8_t pubkey_x[32], const uint8_t pubkey_y[32],
                             const uint8_t* sig_der, uint32_t sig_der_len,
                             const uint8_t* msg, uint32_t msg_len);

int ecdsa_selftest(void);

#endif
