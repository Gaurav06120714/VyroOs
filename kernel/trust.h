#ifndef TRUST_H
#define TRUST_H

#include "../include/types.h"
#include "x509.h"

#define TRUST_MAX 8
#define TRUST_DER_MAX 2048

int  trust_add(const uint8_t* der, uint32_t len);
int  trust_count(void);

int  trust_find_by_subject_cn(const char* issuer_cn,
                              const x509_cert_t** anchor_out,
                              const uint8_t** der_out, uint32_t* der_len_out);

#endif
