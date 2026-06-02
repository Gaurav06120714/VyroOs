#ifndef TLS_H
#define TLS_H

#include "../include/types.h"

// Minimal TLS 1.3 client primitives.
// This phase ships the building blocks: record framing, ClientHello build,
// ServerHello parse, key schedule (RFC 8446 §7.1). Network round-trip lands
// in the next phase.

#define TLS_RECORD_HANDSHAKE   0x16
#define TLS_RECORD_APP_DATA    0x17
#define TLS_RECORD_ALERT       0x15
#define TLS_RECORD_CCS         0x14

#define TLS_HS_CLIENT_HELLO    0x01
#define TLS_HS_SERVER_HELLO    0x02

// RFC 8446 cipher suite: TLS_CHACHA20_POLY1305_SHA256
#define TLS_CIPHER_CHACHA20_POLY1305_SHA256 0x1303

// RFC 8446 supported_groups: x25519
#define TLS_GROUP_X25519                    0x001d

// Build a TLS 1.3 ClientHello record (handshake-wrapped) into `out`.
// Inputs: client_random[32], client_public_x25519[32], session_id[32] (any random),
// hostname (SNI; pass NULL or "" to omit). Returns total length on success, -1 on overflow.
int tls_build_client_hello(uint8_t* out, uint32_t out_max,
                           const uint8_t client_random[32],
                           const uint8_t session_id[32],
                           const uint8_t client_pub_x25519[32],
                           const char* hostname);

// Parse a ServerHello handshake message (just the handshake body, no record header).
// On success fills server_pub_x25519 with the server's key_share and returns 1.
int tls_parse_server_hello(const uint8_t* hs_body, uint32_t hs_body_len,
                           uint8_t server_pub_x25519[32]);

// TLS 1.3 key schedule (RFC 8446 §7.1).
// Given the X25519 shared secret and the SHA-256 hash of (ClientHello || ServerHello),
// computes the handshake secret and the server/client handshake traffic secrets, plus
// the AEAD key (32) and IV (12) for each direction.
typedef struct {
    uint8_t handshake_secret[32];
    uint8_t client_hs_traffic_secret[32];
    uint8_t server_hs_traffic_secret[32];
    uint8_t client_key[32], client_iv[12];
    uint8_t server_key[32], server_iv[12];
} tls_handshake_keys_t;

void tls_derive_handshake_keys(const uint8_t shared_secret[32],
                               const uint8_t ch_sh_hash[32],
                               tls_handshake_keys_t* out);

// Selftest against RFC 8448 §3 (Simple 1-RTT Handshake) published bytes.
int tls_selftest(void);

#endif
