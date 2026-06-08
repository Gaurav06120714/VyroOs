#include "rsa_pss.h"
#include "bignum.h"
#include "bignum_4k.h"
#include "sha256.h"

#define HLEN 32
#define SLEN 32

static void mgf1_sha256(const uint8_t* seed, uint32_t seed_len,
                        uint8_t* out, uint32_t out_len) {
    uint8_t buf[256];
    uint32_t off = 0;
    uint32_t counter = 0;
    while (off < out_len) {
        uint8_t in[64 + 4];
        for (uint32_t i = 0; i < seed_len; i++) in[i] = seed[i];
        in[seed_len + 0] = (uint8_t)(counter >> 24);
        in[seed_len + 1] = (uint8_t)(counter >> 16);
        in[seed_len + 2] = (uint8_t)(counter >> 8);
        in[seed_len + 3] = (uint8_t)(counter);
        sha256(in, seed_len + 4, buf);
        uint32_t take = out_len - off;
        if (take > 32) take = 32;
        for (uint32_t i = 0; i < take; i++) out[off + i] = buf[i];
        off += take;
        counter++;
    }
}

static int pss_em_verify(const uint8_t* em, uint32_t em_len,
                         const uint8_t* msg, uint32_t msg_len) {
    if (em_len < HLEN + SLEN + 2) return 0;
    if (em[em_len - 1] != 0xBC) return 0;
    uint32_t db_len = em_len - HLEN - 1;
    const uint8_t* maskedDB = em;
    const uint8_t* H = em + db_len;


    static uint8_t dbMask[512];
    if (db_len > sizeof(dbMask)) return 0;
    mgf1_sha256(H, HLEN, dbMask, db_len);

    static uint8_t DB[512];
    for (uint32_t i = 0; i < db_len; i++) DB[i] = maskedDB[i] ^ dbMask[i];


    DB[0] &= 0x7F;


    uint32_t ps_len = db_len - SLEN - 1;
    for (uint32_t i = 0; i < ps_len; i++) if (DB[i] != 0) return 0;
    if (DB[ps_len] != 0x01) return 0;
    const uint8_t* salt = DB + ps_len + 1;


    uint8_t mhash[32];
    sha256(msg, msg_len, mhash);
    uint8_t mprime[8 + 32 + 64];
    for (int i = 0; i < 8; i++)   mprime[i]     = 0;
    for (int i = 0; i < 32; i++)  mprime[8 + i] = mhash[i];
    for (int i = 0; i < SLEN; i++) mprime[40 + i] = salt[i];
    uint8_t hprime[32];
    sha256(mprime, 8 + 32 + SLEN, hprime);

    uint8_t diff = 0;
    for (int i = 0; i < 32; i++) diff |= H[i] ^ hprime[i];
    return diff == 0;
}

int rsa_pss_sha256_verify(const uint8_t* n, uint32_t n_len,
                          const uint8_t* e, uint32_t e_len,
                          const uint8_t* sig, uint32_t sig_len,
                          const uint8_t* msg, uint32_t msg_len) {
    if (n_len == 0 || sig_len != n_len) return 0;
    static uint8_t em[BN4K_BYTES];
    if (n_len > sizeof(em)) return 0;

    if (n_len <= BN_BYTES) {
        bn_t mod, exp, s, r;
        bn_from_be(mod, n, n_len);
        bn_from_be(exp, e, e_len);
        bn_from_be(s,   sig, sig_len);
        if (bn_cmp(s, mod) >= 0) return 0;
        bn_modexp(r, s, exp, mod);
        bn_to_be(em, n_len, r);
    } else {
        bn4k_t mod, exp, s, r;
        bn4k_from_be(mod, n, n_len);
        bn4k_from_be(exp, e, e_len);
        bn4k_from_be(s,   sig, sig_len);
        if (bn4k_cmp(s, mod) >= 0) return 0;
        bn4k_modexp(r, s, exp, mod);
        bn4k_to_be(em, n_len, r);
    }
    return pss_em_verify(em, n_len, msg, msg_len);
}
