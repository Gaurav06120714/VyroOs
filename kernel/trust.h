#ifndef TRUST_H
#define TRUST_H

#include "../include/types.h"
#include "x509.h"

// Small static trust-anchor store. Up to TRUST_MAX certificates can be added
// at runtime; tls_connect's chain walker will check the topmost intermediate
// against this store. A real OS bundles the Mozilla CA roots — that's a
// substantial source dump (~250 certs × ~1 KB) that doesn't belong in this
// minimal kernel. We expose the API so a future userspace can populate it.

#define TRUST_MAX 8
#define TRUST_DER_MAX 2048

int  trust_add(const uint8_t* der, uint32_t len);
int  trust_count(void);

// Find a trust anchor whose Subject CN matches `issuer_cn`. Returns 1 on hit.
int  trust_find_by_subject_cn(const char* issuer_cn,
                              const x509_cert_t** anchor_out,
                              const uint8_t** der_out, uint32_t* der_len_out);

#endif
