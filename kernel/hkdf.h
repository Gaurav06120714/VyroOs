#ifndef HKDF_H
#define HKDF_H

#include "../include/types.h"

void hmac_sha256(const uint8_t* key, uint32_t key_len,
                 const uint8_t* msg, uint32_t msg_len,
                 uint8_t out[32]);

void hkdf_extract(const uint8_t* salt, uint32_t salt_len,
                  const uint8_t* ikm,  uint32_t ikm_len,
                  uint8_t prk[32]);

void hkdf_expand(const uint8_t prk[32],
                 const uint8_t* info, uint32_t info_len,
                 uint8_t* out, uint32_t out_len);

void tls13_hkdf_expand_label(const uint8_t prk[32],
                             const char* label_suffix,
                             const uint8_t* ctx, uint32_t ctx_len,
                             uint8_t* out, uint32_t out_len);

void tls13_derive_secret(const uint8_t prk[32],
                         const char* label_suffix,
                         const uint8_t* messages, uint32_t messages_len,
                         uint8_t out[32]);

int hkdf_selftest(void);

#endif
