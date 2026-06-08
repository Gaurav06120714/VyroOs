#include "bignum_4k.h"
#include "sha256.h"

static void zero(bn4k_t r) { for (int i = 0; i < BN4K_LIMBS; i++) r[i] = 0; }
static void copy(bn4k_t r, const bn4k_t a) { for (int i = 0; i < BN4K_LIMBS; i++) r[i] = a[i]; }

int bn4k_is_zero(const bn4k_t a) {
    for (int i = 0; i < BN4K_LIMBS; i++) if (a[i]) return 0;
    return 1;
}

int bn4k_cmp(const bn4k_t a, const bn4k_t b) {
    for (int i = BN4K_LIMBS - 1; i >= 0; i--) {
        if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
    }
    return 0;
}

void bn4k_from_be(bn4k_t r, const uint8_t* be, uint32_t len) {
    zero(r);
    uint32_t i = 0;
    for (uint32_t k = len; k > 0; k--, i++) {
        uint32_t limb = i / 4;
        uint32_t shift = (i % 4) * 8;
        if (limb < BN4K_LIMBS) r[limb] |= ((uint32_t)be[k - 1]) << shift;
    }
}

void bn4k_to_be(uint8_t* out, uint32_t out_len, const bn4k_t a) {
    for (uint32_t i = 0; i < out_len; i++) {
        uint32_t src = out_len - 1 - i;
        uint32_t limb = src / 4;
        uint32_t sh = (src % 4) * 8;
        out[i] = limb < BN4K_LIMBS ? (uint8_t)(a[limb] >> sh) : 0;
    }
}

static uint32_t sub_in_place(bn4k_t r, const bn4k_t b) {
    int64_t borrow = 0;
    for (int i = 0; i < BN4K_LIMBS; i++) {
        int64_t d = (int64_t)r[i] - b[i] - borrow;
        r[i] = (uint32_t)d;
        borrow = (d < 0) ? 1 : 0;
    }
    return (uint32_t)borrow;
}

typedef uint32_t bn4k2_t[BN4K_LIMBS * 2];

static void bn4k2_mul(bn4k2_t r, const bn4k_t a, const bn4k_t b) {
    for (int i = 0; i < BN4K_LIMBS * 2; i++) r[i] = 0;
    for (int i = 0; i < BN4K_LIMBS; i++) {
        uint64_t carry = 0;
        for (int j = 0; j < BN4K_LIMBS; j++) {
            uint64_t t = (uint64_t)r[i + j] + (uint64_t)a[i] * b[j] + carry;
            r[i + j] = (uint32_t)t;
            carry = t >> 32;
        }
        r[i + BN4K_LIMBS] = (uint32_t)carry;
    }
}

static void bn4k2_mod(bn4k_t r, const bn4k2_t prod, const bn4k_t m) {
    bn4k_t rem;
    zero(rem);
    for (int bit = BN4K_LIMBS * 2 * 32 - 1; bit >= 0; bit--) {
        uint32_t carry = 0;
        for (int i = 0; i < BN4K_LIMBS; i++) {
            uint32_t hi = rem[i] >> 31;
            rem[i] = (rem[i] << 1) | carry;
            carry = hi;
        }
        uint32_t overflow = carry;
        rem[0] |= (prod[bit >> 5] >> (bit & 31)) & 1u;
        if (overflow || bn4k_cmp(rem, m) >= 0) sub_in_place(rem, m);
    }
    copy(r, rem);
}

static void bn4k_mulmod(bn4k_t r, const bn4k_t a, const bn4k_t b, const bn4k_t m) {
    bn4k2_t prod;
    bn4k2_mul(prod, a, b);
    bn4k2_mod(r, prod, m);
}

static int bn4k_bit(const bn4k_t a, uint32_t bit) {
    return (a[bit >> 5] >> (bit & 31)) & 1;
}

void bn4k_modexp(bn4k_t r, const bn4k_t base, const bn4k_t exp, const bn4k_t m) {
    bn4k_t result, b;
    zero(result); result[0] = 1;
    copy(b, base);
    int top = -1;
    for (int i = BN4K_BITS - 1; i >= 0; i--) {
        if (bn4k_bit(exp, (uint32_t)i)) { top = i; break; }
    }
    if (top < 0) { copy(r, result); return; }
    for (int i = 0; i <= top; i++) {
        if (bn4k_bit(exp, (uint32_t)i)) {
            bn4k_t tmp;
            bn4k_mulmod(tmp, result, b, m);
            copy(result, tmp);
        }
        if (i < top) {
            bn4k_t tmp;
            bn4k_mulmod(tmp, b, b, m);
            copy(b, tmp);
        }
    }
    copy(r, result);
}

static const uint8_t SHA256_PREFIX[19] = {
    0x30, 0x31, 0x30, 0x0d, 0x06, 0x09,
    0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01,
    0x05, 0x00, 0x04, 0x20
};

int rsa4k_pkcs1_v15_sha256_verify(const uint8_t* n, uint32_t n_len,
                                  const uint8_t* e, uint32_t e_len,
                                  const uint8_t* sig, uint32_t sig_len,
                                  const uint8_t* msg, uint32_t msg_len) {
    if (n_len == 0 || n_len > BN4K_BYTES) return 0;
    if (sig_len != n_len) return 0;
    bn4k_t mod, exp, s, r;
    bn4k_from_be(mod, n, n_len);
    bn4k_from_be(exp, e, e_len);
    bn4k_from_be(s, sig, sig_len);
    if (bn4k_cmp(s, mod) >= 0) return 0;
    bn4k_modexp(r, s, exp, mod);
    static uint8_t em[BN4K_BYTES];
    bn4k_to_be(em, n_len, r);
    if (n_len < 11 + 19 + 32) return 0;
    if (em[0] != 0x00 || em[1] != 0x01) return 0;
    uint32_t p = 2;
    while (p < n_len && em[p] == 0xFF) p++;
    if (p < 10 || p >= n_len) return 0;
    if (em[p] != 0x00) return 0;
    p++;
    if (n_len - p != 19 + 32) return 0;
    for (int i = 0; i < 19; i++) if (em[p + i] != SHA256_PREFIX[i]) return 0;
    p += 19;
    uint8_t h[32];
    sha256(msg, msg_len, h);
    uint8_t diff = 0;
    for (int i = 0; i < 32; i++) diff |= em[p + i] ^ h[i];
    return diff == 0;
}
