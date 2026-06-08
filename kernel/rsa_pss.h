#ifndef RSA_PSS_H
#define RSA_PSS_H

#include "../include/types.h"

int rsa_pss_sha256_verify(const uint8_t* n, uint32_t n_len,
                          const uint8_t* e, uint32_t e_len,
                          const uint8_t* sig, uint32_t sig_len,
                          const uint8_t* msg, uint32_t msg_len);

#endif
