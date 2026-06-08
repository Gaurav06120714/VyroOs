#include "rsa.h"
#include "bignum.h"
#include "sha256.h"

static const uint8_t SHA256_DIGEST_PREFIX[19] = {
    0x30, 0x31, 0x30, 0x0d, 0x06, 0x09,
    0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01,
    0x05, 0x00,
    0x04, 0x20
};

int rsa_pkcs1_v15_sha256_verify(const uint8_t* n, uint32_t n_len,
                                const uint8_t* e, uint32_t e_len,
                                const uint8_t* sig, uint32_t sig_len,
                                const uint8_t* msg, uint32_t msg_len) {
    if (!n || !e || !sig || !msg) return 0;
    if (n_len == 0 || n_len > BN_BYTES) return 0;
    if (sig_len != n_len) return 0;


    bn_t mod, exp, s, r;
    bn_from_be(mod, n, n_len);
    bn_from_be(exp, e, e_len);
    bn_from_be(s,   sig, sig_len);

    if (bn_cmp(s, mod) >= 0) return 0;


    bn_modexp(r, s, exp, mod);


    uint8_t em[BN_BYTES];
    bn_to_be(em, n_len, r);



    if (n_len < 11 + sizeof(SHA256_DIGEST_PREFIX) + 32) return 0;
    if (em[0] != 0x00 || em[1] != 0x01) return 0;
    uint32_t p = 2;
    while (p < n_len && em[p] == 0xFF) p++;
    if (p < 10 || p >= n_len) return 0;
    if (em[p] != 0x00) return 0;
    p++;
    if (n_len - p != sizeof(SHA256_DIGEST_PREFIX) + 32) return 0;
    for (uint32_t i = 0; i < sizeof(SHA256_DIGEST_PREFIX); i++) {
        if (em[p + i] != SHA256_DIGEST_PREFIX[i]) return 0;
    }
    p += sizeof(SHA256_DIGEST_PREFIX);


    uint8_t h[32];
    sha256(msg, msg_len, h);
    uint8_t diff = 0;
    for (int i = 0; i < 32; i++) diff |= em[p + i] ^ h[i];
    return diff == 0;
}

int rsa_selftest(void) {


    bn_t base, exp, mod, r;
    uint8_t base_b[1] = { 3 };
    uint8_t exp_b[1]  = { 7 };
    uint8_t mod_b[1]  = { 100 };
    bn_from_be(base, base_b, 1);
    bn_from_be(exp,  exp_b,  1);
    bn_from_be(mod,  mod_b,  1);
    bn_modexp(r, base, exp, mod);
    uint8_t out[1];
    bn_to_be(out, 1, r);
    if (out[0] != 87) return 0;


    uint8_t base2[1] = { 2 };
    uint8_t exp2[1]  = { 10 };
    uint8_t mod2[2]  = { 0x03, 0xE8 };
    bn_from_be(base, base2, 1);
    bn_from_be(exp,  exp2,  1);
    bn_from_be(mod,  mod2,  2);
    bn_modexp(r, base, exp, mod);
    uint8_t out2[2];
    bn_to_be(out2, 2, r);
    if (out2[0] != 0 || out2[1] != 24) return 0;

    return 1;
}
