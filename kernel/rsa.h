#ifndef RSA_H
#define RSA_H

#include "../include/types.h"

int rsa_pkcs1_v15_sha256_verify(const uint8_t* n, uint32_t n_len,
                                const uint8_t* e, uint32_t e_len,
                                const uint8_t* sig, uint32_t sig_len,
                                const uint8_t* msg, uint32_t msg_len);

int rsa_selftest(void);

#endif
