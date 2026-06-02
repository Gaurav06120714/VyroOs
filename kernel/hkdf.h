#ifndef HKDF_H
#define HKDF_H

#include "../include/types.h"

// HMAC-SHA-256 (RFC 2104). out is always 32 bytes.
void hmac_sha256(const uint8_t* key, uint32_t key_len,
                 const uint8_t* msg, uint32_t msg_len,
                 uint8_t out[32]);

// HKDF-Extract (RFC 5869 §2.2). PRK is 32 bytes. If salt is NULL or salt_len==0,
// 32 zero bytes are used.
void hkdf_extract(const uint8_t* salt, uint32_t salt_len,
                  const uint8_t* ikm,  uint32_t ikm_len,
                  uint8_t prk[32]);

// HKDF-Expand (RFC 5869 §2.3). Writes `out_len` bytes (<= 255*32 = 8160).
void hkdf_expand(const uint8_t prk[32],
                 const uint8_t* info, uint32_t info_len,
                 uint8_t* out, uint32_t out_len);

// TLS 1.3 HKDF-Expand-Label (RFC 8446 §7.1):
//   HkdfLabel = struct { uint16 length; opaque label<7..255>; opaque ctx<0..255>; }
//   label is the suffix after "tls13 ".
void tls13_hkdf_expand_label(const uint8_t prk[32],
                             const char* label_suffix,
                             const uint8_t* ctx, uint32_t ctx_len,
                             uint8_t* out, uint32_t out_len);

// TLS 1.3 Derive-Secret(Secret, Label, Messages):
//   = HKDF-Expand-Label(Secret, Label, SHA256(Messages), 32)
void tls13_derive_secret(const uint8_t prk[32],
                         const char* label_suffix,
                         const uint8_t* messages, uint32_t messages_len,
                         uint8_t out[32]);

// Selftest: HMAC RFC 4231 test 1, HKDF RFC 5869 test case 1.
int hkdf_selftest(void);

#endif
