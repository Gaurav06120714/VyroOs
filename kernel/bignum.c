#include "bignum.h"

void bn_zero(bn_t r) { for (int i = 0; i < BN_LIMBS; i++) r[i] = 0; }
void bn_copy(bn_t r, const bn_t a) { for (int i = 0; i < BN_LIMBS; i++) r[i] = a[i]; }

int bn_is_zero(const bn_t a) {
    for (int i = 0; i < BN_LIMBS; i++) if (a[i]) return 0;
    return 1;
}

int bn_cmp(const bn_t a, const bn_t b) {
    for (int i = BN_LIMBS - 1; i >= 0; i--) {
        if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
    }
    return 0;
}

void bn_from_be(bn_t r, const uint8_t* be, uint32_t len) {
    bn_zero(r);
    // be[0] is most-significant byte. Place into little-endian limbs.
    uint32_t i = 0;
    for (uint32_t k = len; k > 0; k--, i++) {
        uint32_t limb = i / 4;
        uint32_t shift = (i % 4) * 8;
        if (limb < BN_LIMBS) r[limb] |= ((uint32_t)be[k - 1]) << shift;
    }
}

void bn_to_be(uint8_t* out, uint32_t out_len, const bn_t a) {
    for (uint32_t i = 0; i < out_len; i++) {
        uint32_t src_byte = out_len - 1 - i;          // little-endian byte index
        uint32_t limb = src_byte / 4;
        uint32_t shift = (src_byte % 4) * 8;
        out[i] = limb < BN_LIMBS ? (uint8_t)(a[limb] >> shift) : 0;
    }
}

static uint32_t add_in_place(bn_t r, const bn_t b) {
    uint64_t carry = 0;
    for (int i = 0; i < BN_LIMBS; i++) {
        uint64_t s = (uint64_t)r[i] + b[i] + carry;
        r[i] = (uint32_t)s;
        carry = s >> 32;
    }
    return (uint32_t)carry;
}

static uint32_t sub_in_place(bn_t r, const bn_t b) {
    int64_t borrow = 0;
    for (int i = 0; i < BN_LIMBS; i++) {
        int64_t d = (int64_t)r[i] - b[i] - borrow;
        r[i] = (uint32_t)d;
        borrow = (d < 0) ? 1 : 0;
    }
    return (uint32_t)borrow;
}

void bn_addmod(bn_t r, const bn_t a, const bn_t b, const bn_t m) {
    bn_copy(r, a);
    uint32_t carry = add_in_place(r, b);
    if (carry || bn_cmp(r, m) >= 0) sub_in_place(r, m);
}

// 2 × N-limb integer for the multiplication output.
typedef uint32_t bn2_t[BN_LIMBS * 2];

static void bn2_zero(bn2_t r) { for (int i = 0; i < BN_LIMBS * 2; i++) r[i] = 0; }

static void bn2_shl1(bn2_t r) {
    uint32_t carry = 0;
    for (int i = 0; i < BN_LIMBS * 2; i++) {
        uint32_t hi = r[i] >> 31;
        r[i] = (r[i] << 1) | carry;
        carry = hi;
    }
}

static int bn2_geq_shifted(const bn2_t r, const bn_t m, int shift_limbs) {
    // Compare r vs m << (shift_limbs * 32). Return 1 if r >= shifted m.
    // m occupies r[shift_limbs..shift_limbs + N - 1]. Higher limbs of r must be 0.
    for (int i = BN_LIMBS * 2 - 1; i >= shift_limbs + BN_LIMBS; i--) {
        if (r[i]) return 1;
    }
    for (int i = BN_LIMBS - 1; i >= 0; i--) {
        uint32_t ri = r[shift_limbs + i];
        if (ri != m[i]) return ri > m[i] ? 1 : 0;
    }
    return 1;        // equal
}

static void bn2_sub_shifted(bn2_t r, const bn_t m, int shift_limbs) {
    int64_t borrow = 0;
    for (int i = 0; i < BN_LIMBS; i++) {
        int64_t d = (int64_t)r[shift_limbs + i] - m[i] - borrow;
        r[shift_limbs + i] = (uint32_t)d;
        borrow = (d < 0) ? 1 : 0;
    }
    for (int i = shift_limbs + BN_LIMBS; i < BN_LIMBS * 2 && borrow; i++) {
        int64_t d = (int64_t)r[i] - borrow;
        r[i] = (uint32_t)d;
        borrow = (d < 0) ? 1 : 0;
    }
}

static void bn2_mul(bn2_t r, const bn_t a, const bn_t b) {
    bn2_zero(r);
    for (int i = 0; i < BN_LIMBS; i++) {
        uint64_t carry = 0;
        for (int j = 0; j < BN_LIMBS; j++) {
            uint64_t t = (uint64_t)r[i + j] + (uint64_t)a[i] * b[j] + carry;
            r[i + j] = (uint32_t)t;
            carry = t >> 32;
        }
        r[i + BN_LIMBS] = (uint32_t)carry;
    }
}

static void bn2_mod(bn_t r, const bn2_t prod, const bn_t m) {
    // Binary long division. For each bit of `prod` from MSB down to LSB:
    //   remainder = (remainder << 1) | bit;  if (remainder >= m) remainder -= m;
    // Bounded at 4096 iterations × O(N) — no runaway loops.
    bn_t rem;
    bn_zero(rem);
    for (int bit = BN_LIMBS * 2 * 32 - 1; bit >= 0; bit--) {
        // shift rem left by 1 (track the bit shifted past limb 63 — that
        // represents an implicit bit-2048 of rem that bn_cmp can't see).
        uint32_t carry = 0;
        for (int i = 0; i < BN_LIMBS; i++) {
            uint32_t hi = rem[i] >> 31;
            rem[i] = (rem[i] << 1) | carry;
            carry = hi;
        }
        uint32_t overflow = carry;
        rem[0] |= (prod[bit >> 5] >> (bit & 31)) & 1u;
        if (overflow || bn_cmp(rem, m) >= 0) sub_in_place(rem, m);
    }
    bn_copy(r, rem);
    (void)bn2_geq_shifted; (void)bn2_sub_shifted; (void)bn2_shl1;
}

void bn_mulmod(bn_t r, const bn_t a, const bn_t b, const bn_t m) {
    bn2_t prod;
    bn2_mul(prod, a, b);
    bn2_mod(r, prod, m);
}

// Bit accessor (MSB = bit BN_BITS-1).
static int bn_bit(const bn_t a, uint32_t bit) {
    return (a[bit >> 5] >> (bit & 31)) & 1;
}

void bn_modexp(bn_t r, const bn_t base, const bn_t exp, const bn_t m) {
    // Right-to-left square-and-multiply.
    bn_t result, b;
    bn_zero(result); result[0] = 1;
    bn_copy(b, base);
    // Find the topmost set bit of exp to avoid full 2048-bit loop.
    int top = -1;
    for (int i = BN_BITS - 1; i >= 0; i--) {
        if (bn_bit(exp, (uint32_t)i)) { top = i; break; }
    }
    if (top < 0) { bn_copy(r, result); return; }
    for (int i = 0; i <= top; i++) {
        if (bn_bit(exp, (uint32_t)i)) {
            bn_t tmp;
            bn_mulmod(tmp, result, b, m);
            bn_copy(result, tmp);
        }
        if (i < top) {
            bn_t tmp;
            bn_mulmod(tmp, b, b, m);
            bn_copy(b, tmp);
        }
    }
    bn_copy(r, result);
}
