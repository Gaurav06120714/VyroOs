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

// Parse a ClientHello handshake message (just the handshake body, no record header).
// On success fills client_pub_x25519 with the client's X25519 key_share and
// `out_chacha_ok` with whether TLS_CHACHA20_POLY1305_SHA256 was offered.
// Returns 1 on success, 0 if the message is malformed or required extensions
// (supported_versions=0x0304, key_share=x25519) are absent.
int tls_parse_client_hello(const uint8_t* hs_body, uint32_t hs_body_len,
                           uint8_t client_pub_x25519[32],
                           uint8_t client_random[32],
                           uint8_t session_id[32], uint8_t* sid_len,
                           int* out_chacha_ok);

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

// ─── TLS connection state machine (v3.11) ───
enum tls_conn_state {
    TLS_CS_INIT = 0,
    TLS_CS_CH_SENT,
    TLS_CS_SH_RECEIVED,
    TLS_CS_FINISHED_OK,
    TLS_CS_ERROR
};

#define TLS_TRANSCRIPT_MAX  8192
#define TLS_RX_BUF_MAX     16384

typedef struct {
    int      tcp_id;
    char     hostname[64];

    uint8_t  client_priv[32];
    uint8_t  client_pub[32];
    uint8_t  client_random[32];
    uint8_t  session_id[32];

    // Transcript: raw bytes of every handshake message in order (no record header).
    uint8_t  transcript[TLS_TRANSCRIPT_MAX];
    uint32_t transcript_len;

    // Inbound TCP byte buffer (TCP can deliver record-fragmented).
    uint8_t  rx_buf[TLS_RX_BUF_MAX];
    uint32_t rx_len;

    // Derived after ServerHello.
    tls_handshake_keys_t keys;
    uint64_t server_seq;
    uint64_t client_seq;             // for sending encrypted handshake (client Finished)
    uint8_t  saw_server_finished;

    // Result of server-Finished MAC check.
    uint8_t  finished_mac_ok;

    // Application traffic keys/IVs (derived after handshake completes).
    uint8_t  client_ap_key[32], client_ap_iv[12];
    uint8_t  server_ap_key[32], server_ap_iv[12];
    uint64_t client_ap_seq;
    uint64_t server_ap_seq;

    // Certificate parsing + verification result (set during handshake).
    uint8_t  saw_certificate;
    uint8_t  cert_parse_ok;
    uint8_t  cert_self_sign_ok;       // 1 if leaf signature verifies under leaf's own pubkey
    uint8_t  cert_chain_verified;     // 1 if chain links all verified + terminated at trust anchor
    uint8_t  cert_chain_len;          // number of certs in the received chain
    uint8_t  hostname_match_ok;       // 1 if hostname matched leaf's CN or any SAN
    char     cert_subject_cn[128];
    char     cert_issuer_cn[128];
    uint8_t  cert_sig_alg;
    uint8_t  cert_pkey_alg;

    uint8_t  state;
} tls_ctx_t;

// Send application data over the established TLS connection (encrypted record).
// Returns bytes sent, or < 0 on error.
int tls_send(tls_ctx_t* ctx, const uint8_t* data, uint32_t len);

// Receive application data. Drains TCP, decrypts records, returns bytes.
int tls_recv(tls_ctx_t* ctx, uint8_t* out, uint32_t max, uint32_t timeout_ms);

// Run a full TLS 1.3 handshake on an already-ESTABLISHED TCP conn.
// Returns 1 if we reached TLS_CS_FINISHED_OK with a valid server Finished MAC.
int tls_connect(tls_ctx_t* ctx, int tcp_id, const char* hostname,
                uint32_t timeout_ms);

const char* tls_state_name(uint8_t s);

#endif
