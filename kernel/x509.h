#ifndef X509_H
#define X509_H

#include "../include/types.h"

enum {
    X509_SIG_UNKNOWN        = 0,
    X509_SIG_SHA256_RSA     = 1,
    X509_SIG_SHA384_RSA     = 2,
    X509_SIG_SHA512_RSA     = 3,
    X509_SIG_ECDSA_SHA256   = 4,
    X509_SIG_ECDSA_SHA384   = 5,
    X509_SIG_RSA_PSS_SHA256 = 6
};

enum {
    X509_PKEY_UNKNOWN = 0,
    X509_PKEY_RSA     = 1,
    X509_PKEY_EC      = 2
};

#define X509_CN_MAX     128
#define X509_TIME_MAX    32
#define X509_SAN_MAX      8
#define X509_SAN_LEN_MAX 128
#define X509_PUBKEY_N_MAX 512
#define X509_PUBKEY_E_MAX   8

typedef struct {
    char    subject_cn[X509_CN_MAX];
    char    issuer_cn [X509_CN_MAX];
    char    not_before[X509_TIME_MAX];
    char    not_after [X509_TIME_MAX];
    uint8_t san_count;
    char    san[X509_SAN_MAX][X509_SAN_LEN_MAX];
    uint8_t sig_alg;
    uint8_t pkey_alg;


    uint8_t  pubkey_n[X509_PUBKEY_N_MAX];
    uint32_t pubkey_n_len;
    uint8_t  pubkey_e[X509_PUBKEY_E_MAX];
    uint32_t pubkey_e_len;


    uint8_t  pubkey_ec_x[32];
    uint8_t  pubkey_ec_y[32];
    uint8_t  pubkey_ec_valid;


    uint32_t tbs_off, tbs_len;
    uint32_t sig_off, sig_len;
} x509_cert_t;

int x509_verify_signature(const uint8_t* child_der, uint32_t child_der_len,
                          const x509_cert_t* child, const x509_cert_t* parent);

int x509_parse(const uint8_t* der, uint32_t len, x509_cert_t* out);

int x509_selftest(void);

const char* x509_sig_alg_name(uint8_t e);
const char* x509_pkey_alg_name(uint8_t e);

#endif
