#ifndef X509_H
#define X509_H

#include "../include/types.h"

// Recognized signature algorithms.
enum {
    X509_SIG_UNKNOWN        = 0,
    X509_SIG_SHA256_RSA     = 1,    // 1.2.840.113549.1.1.11
    X509_SIG_SHA384_RSA     = 2,    // 1.2.840.113549.1.1.12
    X509_SIG_SHA512_RSA     = 3,    // 1.2.840.113549.1.1.13
    X509_SIG_ECDSA_SHA256   = 4,    // 1.2.840.10045.4.3.2
    X509_SIG_ECDSA_SHA384   = 5     // 1.2.840.10045.4.3.3
};

// Recognized public-key algorithms.
enum {
    X509_PKEY_UNKNOWN = 0,
    X509_PKEY_RSA     = 1,          // 1.2.840.113549.1.1.1
    X509_PKEY_EC      = 2           // 1.2.840.10045.2.1
};

#define X509_CN_MAX     128
#define X509_TIME_MAX    32
#define X509_SAN_MAX      8
#define X509_SAN_LEN_MAX 128
#define X509_PUBKEY_N_MAX 256          // RSA-2048 modulus
#define X509_PUBKEY_E_MAX   8

typedef struct {
    char    subject_cn[X509_CN_MAX];
    char    issuer_cn [X509_CN_MAX];
    char    not_before[X509_TIME_MAX];   // raw ASCII, UTCTime or GeneralizedTime
    char    not_after [X509_TIME_MAX];
    uint8_t san_count;
    char    san[X509_SAN_MAX][X509_SAN_LEN_MAX];
    uint8_t sig_alg;
    uint8_t pkey_alg;

    // RSA public key (big-endian, as in DER).
    uint8_t  pubkey_n[X509_PUBKEY_N_MAX];
    uint32_t pubkey_n_len;
    uint8_t  pubkey_e[X509_PUBKEY_E_MAX];
    uint32_t pubkey_e_len;

    // Offsets within the original DER buffer.
    uint32_t tbs_off, tbs_len;
    uint32_t sig_off, sig_len;
} x509_cert_t;

// Verify `child`'s signature using `parent`'s public key.
// Both must have been parsed from a known-good DER buffer; the caller
// passes the child's original DER buffer so we can hash tbsCertificate.
// Returns 1 if signature verifies.
int x509_verify_signature(const uint8_t* child_der, uint32_t child_der_len,
                          const x509_cert_t* child, const x509_cert_t* parent);

// Parse a DER-encoded X.509 v3 certificate. Returns 1 on success.
int x509_parse(const uint8_t* der, uint32_t len, x509_cert_t* out);

// Self-test: parse the embedded ECDSA test cert and verify identity fields.
int x509_selftest(void);

// Constant strings for the recognized algorithm enums (for shell display).
const char* x509_sig_alg_name(uint8_t e);
const char* x509_pkey_alg_name(uint8_t e);

#endif
