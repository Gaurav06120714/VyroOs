#include "trust.h"

static struct {
    uint8_t      der[TRUST_DER_MAX];
    uint32_t     der_len;
    x509_cert_t  parsed;
    uint8_t      active;
} store[TRUST_MAX];

static int n_anchors = 0;

int trust_add(const uint8_t* der, uint32_t len) {
    if (n_anchors >= TRUST_MAX) return 0;
    if (len > TRUST_DER_MAX) return 0;
    int slot = -1;
    for (int i = 0; i < TRUST_MAX; i++) if (!store[i].active) { slot = i; break; }
    if (slot < 0) return 0;
    for (uint32_t i = 0; i < len; i++) store[slot].der[i] = der[i];
    store[slot].der_len = len;
    if (!x509_parse(store[slot].der, len, &store[slot].parsed)) return 0;
    store[slot].active = 1;
    n_anchors++;
    return 1;
}

int trust_count(void) { return n_anchors; }

static int streq(const char* a, const char* b) {
    while (*a && *b) {
        char ca = *a, cb = *b;
        if (ca >= 'A' && ca <= 'Z') ca += 32;
        if (cb >= 'A' && cb <= 'Z') cb += 32;
        if (ca != cb) return 0;
        a++; b++;
    }
    return *a == 0 && *b == 0;
}

int trust_find_by_subject_cn(const char* issuer_cn,
                             const x509_cert_t** anchor_out,
                             const uint8_t** der_out, uint32_t* der_len_out) {
    if (!issuer_cn || !*issuer_cn) return 0;
    for (int i = 0; i < TRUST_MAX; i++) {
        if (!store[i].active) continue;
        if (streq(store[i].parsed.subject_cn, issuer_cn)) {
            if (anchor_out)  *anchor_out  = &store[i].parsed;
            if (der_out)     *der_out     = store[i].der;
            if (der_len_out) *der_len_out = store[i].der_len;
            return 1;
        }
    }
    return 0;
}
