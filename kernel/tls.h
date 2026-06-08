#ifndef TLS_H
#define TLS_H

#include "../include/types.h"

#define TLS_RECORD_HANDSHAKE   0x16
#define TLS_RECORD_APP_DATA    0x17
#define TLS_RECORD_ALERT       0x15
#define TLS_RECORD_CCS         0x14

#define TLS_HS_CLIENT_HELLO    0x01
#define TLS_HS_SERVER_HELLO    0x02

#define TLS_CIPHER_CHACHA20_POLY1305_SHA256 0x1303

#define TLS_GROUP_X25519                    0x001d

int tls_build_client_hello(uint8_t* out, uint32_t out_max,
                           const uint8_t client_random[32],
                           const uint8_t session_id[32],
                           const uint8_t client_pub_x25519[32],
                           const char* hostname);

int tls_parse_server_hello(const uint8_t* hs_body, uint32_t hs_body_len,
                           uint8_t server_pub_x25519[32]);

int tls_build_certificate_msg(uint8_t* out, uint32_t out_max,
                              const uint8_t* cert_der, uint32_t cert_der_len);

int tls_build_server_finished(uint8_t* out, uint32_t out_max,
                              const uint8_t server_hs_traffic_secret[32],
                              const uint8_t transcript_hash[32]);

int tls_build_server_hello(uint8_t* out, uint32_t out_max,
                           const uint8_t server_random[32],
                           const uint8_t session_id[32], uint8_t sid_len,
                           const uint8_t server_pub_x25519[32]);

int tls_parse_client_hello(const uint8_t* hs_body, uint32_t hs_body_len,
                           uint8_t client_pub_x25519[32],
                           uint8_t client_random[32],
                           uint8_t session_id[32], uint8_t* sid_len,
                           int* out_chacha_ok);

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

int tls_selftest(void);

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


    uint8_t  transcript[TLS_TRANSCRIPT_MAX];
    uint32_t transcript_len;


    uint8_t  rx_buf[TLS_RX_BUF_MAX];
    uint32_t rx_len;


    tls_handshake_keys_t keys;
    uint64_t server_seq;
    uint64_t client_seq;
    uint8_t  saw_server_finished;


    uint8_t  finished_mac_ok;


    uint8_t  client_ap_key[32], client_ap_iv[12];
    uint8_t  server_ap_key[32], server_ap_iv[12];
    uint64_t client_ap_seq;
    uint64_t server_ap_seq;


    uint8_t  saw_certificate;
    uint8_t  cert_parse_ok;
    uint8_t  cert_self_sign_ok;
    uint8_t  cert_chain_verified;
    uint8_t  cert_chain_len;
    uint8_t  hostname_match_ok;
    char     cert_subject_cn[128];
    char     cert_issuer_cn[128];
    uint8_t  cert_sig_alg;
    uint8_t  cert_pkey_alg;

    uint8_t  state;
} tls_ctx_t;

int tls_send(tls_ctx_t* ctx, const uint8_t* data, uint32_t len);

int tls_recv(tls_ctx_t* ctx, uint8_t* out, uint32_t max, uint32_t timeout_ms);

int tls_connect(tls_ctx_t* ctx, int tcp_id, const char* hostname,
                uint32_t timeout_ms);

const char* tls_state_name(uint8_t s);

int tls_server_demo(const uint8_t* ch_record, uint32_t ch_record_len,
                    uint8_t out_log[256]);

int tls_accept(tls_ctx_t* ctx, int tcp_id, uint32_t timeout_ms);

#endif
