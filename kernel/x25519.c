#include "x25519.h"

// Curve25519 field arithmetic with 5 × 51-bit limbs (donna-64 style).
// Field is GF(2^255 - 19). Each fe is uint64_t[5], each limb in [0, 2^51+epsilon).

typedef uint64_t fe[5];
typedef unsigned __int128 u128;

static inline void fe_zero(fe r) { r[0]=r[1]=r[2]=r[3]=r[4]=0; }
static inline void fe_one (fe r) { r[0]=1; r[1]=r[2]=r[3]=r[4]=0; }
static inline void fe_copy(fe r, const fe a) { for (int i=0;i<5;i++) r[i]=a[i]; }

static void fe_add(fe r, const fe a, const fe b) {
    for (int i = 0; i < 5; i++) r[i] = a[i] + b[i];
}

// r = a - b in unreduced form. We add 4*p (treated additively, each limb is
// 4 * p_limb_i ≈ 2^53) so r remains positive even when a, b are unreduced.
static void fe_sub(fe r, const fe a, const fe b) {
    static const uint64_t four_p[5] = {
        0x1FFFFFFFFFFFB4ULL,    // 4*(2^51 - 19) = 2^53 - 76
        0x1FFFFFFFFFFFFCULL,    // 4*(2^51 - 1)  = 2^53 - 4
        0x1FFFFFFFFFFFFCULL,
        0x1FFFFFFFFFFFFCULL,
        0x1FFFFFFFFFFFFCULL
    };
    for (int i = 0; i < 5; i++) r[i] = a[i] + four_p[i] - b[i];
}

// Reduce a 5-limb element so every limb fits in 51 bits.
static void fe_carry(fe r) {
    uint64_t c;
    c = r[0] >> 51; r[0] &= 0x7FFFFFFFFFFFFULL; r[1] += c;
    c = r[1] >> 51; r[1] &= 0x7FFFFFFFFFFFFULL; r[2] += c;
    c = r[2] >> 51; r[2] &= 0x7FFFFFFFFFFFFULL; r[3] += c;
    c = r[3] >> 51; r[3] &= 0x7FFFFFFFFFFFFULL; r[4] += c;
    c = r[4] >> 51; r[4] &= 0x7FFFFFFFFFFFFULL; r[0] += c * 19;
    c = r[0] >> 51; r[0] &= 0x7FFFFFFFFFFFFULL; r[1] += c;
}

static void fe_mul(fe r, const fe a, const fe b) {
    u128 t0,t1,t2,t3,t4;
    uint64_t b0 = b[0], b1 = b[1], b2 = b[2], b3 = b[3], b4 = b[4];
    uint64_t b1_19 = 19 * b1;
    uint64_t b2_19 = 19 * b2;
    uint64_t b3_19 = 19 * b3;
    uint64_t b4_19 = 19 * b4;

    t0 = (u128)a[0]*b0    + (u128)a[1]*b4_19 + (u128)a[2]*b3_19 + (u128)a[3]*b2_19 + (u128)a[4]*b1_19;
    t1 = (u128)a[0]*b1    + (u128)a[1]*b0    + (u128)a[2]*b4_19 + (u128)a[3]*b3_19 + (u128)a[4]*b2_19;
    t2 = (u128)a[0]*b2    + (u128)a[1]*b1    + (u128)a[2]*b0    + (u128)a[3]*b4_19 + (u128)a[4]*b3_19;
    t3 = (u128)a[0]*b3    + (u128)a[1]*b2    + (u128)a[2]*b1    + (u128)a[3]*b0    + (u128)a[4]*b4_19;
    t4 = (u128)a[0]*b4    + (u128)a[1]*b3    + (u128)a[2]*b2    + (u128)a[3]*b1    + (u128)a[4]*b0;

    uint64_t c;
    c = (uint64_t)(t0 >> 51); r[0] = (uint64_t)t0 & 0x7FFFFFFFFFFFFULL; t1 += c;
    c = (uint64_t)(t1 >> 51); r[1] = (uint64_t)t1 & 0x7FFFFFFFFFFFFULL; t2 += c;
    c = (uint64_t)(t2 >> 51); r[2] = (uint64_t)t2 & 0x7FFFFFFFFFFFFULL; t3 += c;
    c = (uint64_t)(t3 >> 51); r[3] = (uint64_t)t3 & 0x7FFFFFFFFFFFFULL; t4 += c;
    c = (uint64_t)(t4 >> 51); r[4] = (uint64_t)t4 & 0x7FFFFFFFFFFFFULL; r[0] += c * 19;
    c = r[0] >> 51;           r[0] &= 0x7FFFFFFFFFFFFULL;               r[1] += c;
}

static void fe_sq(fe r, const fe a) { fe_mul(r, a, a); }

// Multiply by a small integer (used for *121665 in the ladder).
static void fe_mul121665(fe r, const fe a) {
    u128 t0 = (u128)a[0]*121665;
    u128 t1 = (u128)a[1]*121665;
    u128 t2 = (u128)a[2]*121665;
    u128 t3 = (u128)a[3]*121665;
    u128 t4 = (u128)a[4]*121665;
    uint64_t c;
    c = (uint64_t)(t0 >> 51); r[0] = (uint64_t)t0 & 0x7FFFFFFFFFFFFULL; t1 += c;
    c = (uint64_t)(t1 >> 51); r[1] = (uint64_t)t1 & 0x7FFFFFFFFFFFFULL; t2 += c;
    c = (uint64_t)(t2 >> 51); r[2] = (uint64_t)t2 & 0x7FFFFFFFFFFFFULL; t3 += c;
    c = (uint64_t)(t3 >> 51); r[3] = (uint64_t)t3 & 0x7FFFFFFFFFFFFULL; t4 += c;
    c = (uint64_t)(t4 >> 51); r[4] = (uint64_t)t4 & 0x7FFFFFFFFFFFFULL; r[0] += c * 19;
    c = r[0] >> 51;           r[0] &= 0x7FFFFFFFFFFFFULL;               r[1] += c;
}

static void fe_from_bytes(fe r, const uint8_t b[32]) {
    uint64_t t0 =  (uint64_t)b[ 0]        | ((uint64_t)b[ 1] <<  8) |
                  ((uint64_t)b[ 2] << 16) | ((uint64_t)b[ 3] << 24) |
                  ((uint64_t)b[ 4] << 32) | ((uint64_t)b[ 5] << 40) |
                  ((uint64_t)(b[ 6] & 0x07) << 48);
    uint64_t t1 = ((uint64_t)b[ 6] >> 3)  | ((uint64_t)b[ 7] <<  5) |
                  ((uint64_t)b[ 8] << 13) | ((uint64_t)b[ 9] << 21) |
                  ((uint64_t)b[10] << 29) | ((uint64_t)b[11] << 37) |
                  ((uint64_t)(b[12] & 0x3F) << 45);
    uint64_t t2 = ((uint64_t)b[12] >> 6)  | ((uint64_t)b[13] <<  2) |
                  ((uint64_t)b[14] << 10) | ((uint64_t)b[15] << 18) |
                  ((uint64_t)b[16] << 26) | ((uint64_t)b[17] << 34) |
                  ((uint64_t)b[18] << 42) | ((uint64_t)(b[19] & 0x01) << 50);
    uint64_t t3 = ((uint64_t)b[19] >> 1)  | ((uint64_t)b[20] <<  7) |
                  ((uint64_t)b[21] << 15) | ((uint64_t)b[22] << 23) |
                  ((uint64_t)b[23] << 31) | ((uint64_t)b[24] << 39) |
                  ((uint64_t)(b[25] & 0x0F) << 47);
    uint64_t t4 = ((uint64_t)b[25] >> 4)  | ((uint64_t)b[26] <<  4) |
                  ((uint64_t)b[27] << 12) | ((uint64_t)b[28] << 20) |
                  ((uint64_t)b[29] << 28) | ((uint64_t)b[30] << 36) |
                  ((uint64_t)(b[31] & 0x7F) << 44);
    r[0] = t0; r[1] = t1; r[2] = t2; r[3] = t3; r[4] = t4;
}

// Fully reduce and serialize little-endian.
static void fe_to_bytes(uint8_t out[32], const fe a_in) {
    fe a;
    fe_copy(a, a_in);
    fe_carry(a);
    fe_carry(a);
    // Conditionally subtract p = 2^255 - 19. If a >= p, subtract.
    uint64_t t0 = a[0] + 19;
    uint64_t c = t0 >> 51;
    uint64_t t1 = a[1] + c; c = t1 >> 51;
    uint64_t t2 = a[2] + c; c = t2 >> 51;
    uint64_t t3 = a[3] + c; c = t3 >> 51;
    uint64_t t4 = a[4] + c;
    // If t4 has bit 51 set, a >= p — use the reduced form (a + 19) mod 2^255.
    uint64_t mask = -(uint64_t)((t4 >> 51) & 1);
    t4 &= 0x7FFFFFFFFFFFFULL;
    a[0] = (a[0] & ~mask) | ((t0 & 0x7FFFFFFFFFFFFULL) & mask);
    a[1] = (a[1] & ~mask) | ((t1 & 0x7FFFFFFFFFFFFULL) & mask);
    a[2] = (a[2] & ~mask) | ((t2 & 0x7FFFFFFFFFFFFULL) & mask);
    a[3] = (a[3] & ~mask) | ((t3 & 0x7FFFFFFFFFFFFULL) & mask);
    a[4] = (a[4] & ~mask) | (t4 & mask);

    uint64_t s0 = a[0]       | (a[1] << 51);
    uint64_t s1 = (a[1] >> 13) | (a[2] << 38);
    uint64_t s2 = (a[2] >> 26) | (a[3] << 25);
    uint64_t s3 = (a[3] >> 39) | (a[4] << 12);
    for (int i = 0; i < 8; i++) out[ 0 + i] = (uint8_t)(s0 >> (8 * i));
    for (int i = 0; i < 8; i++) out[ 8 + i] = (uint8_t)(s1 >> (8 * i));
    for (int i = 0; i < 8; i++) out[16 + i] = (uint8_t)(s2 >> (8 * i));
    for (int i = 0; i < 8; i++) out[24 + i] = (uint8_t)(s3 >> (8 * i));
}

static void fe_cswap(fe a, fe b, uint64_t swap) {
    uint64_t mask = -swap;
    for (int i = 0; i < 5; i++) {
        uint64_t t = mask & (a[i] ^ b[i]);
        a[i] ^= t;
        b[i] ^= t;
    }
}

// Field inversion: r = a^(p-2) via the standard 2^255 - 21 addition chain.
static void fe_inv(fe r, const fe z) {
    fe z2, z9, z11, z2_5_0, z2_10_0, z2_20_0, z2_50_0, z2_100_0, t;
    fe_sq(z2, z);                                            // z^2
    fe_sq(t, z2); fe_sq(t, t);                                // z^8
    fe_mul(z9, t, z);                                         // z^9
    fe_mul(z11, z9, z2);                                      // z^11
    fe_sq(t, z11); fe_mul(z2_5_0, t, z9);                     // z^(2^5 - 1)
    fe_sq(t, z2_5_0); for (int i = 1; i < 5; i++) fe_sq(t, t);
    fe_mul(z2_10_0, t, z2_5_0);                               // z^(2^10 - 1)
    fe_sq(t, z2_10_0); for (int i = 1; i < 10; i++) fe_sq(t, t);
    fe_mul(z2_20_0, t, z2_10_0);                              // z^(2^20 - 1)
    fe_sq(t, z2_20_0); for (int i = 1; i < 20; i++) fe_sq(t, t);
    fe_mul(t, t, z2_20_0);                                    // z^(2^40 - 1)
    for (int i = 0; i < 10; i++) fe_sq(t, t);
    fe_mul(z2_50_0, t, z2_10_0);                              // z^(2^50 - 1)
    fe_sq(t, z2_50_0); for (int i = 1; i < 50; i++) fe_sq(t, t);
    fe_mul(z2_100_0, t, z2_50_0);                             // z^(2^100 - 1)
    fe_sq(t, z2_100_0); for (int i = 1; i < 100; i++) fe_sq(t, t);
    fe_mul(t, t, z2_100_0);                                   // z^(2^200 - 1)
    for (int i = 0; i < 50; i++) fe_sq(t, t);
    fe_mul(t, t, z2_50_0);                                    // z^(2^250 - 1)
    fe_sq(t, t); fe_sq(t, t); fe_sq(t, t); fe_sq(t, t); fe_sq(t, t);
    fe_mul(r, t, z11);                                        // z^(2^255 - 21)
}

void x25519(uint8_t out[32], const uint8_t scalar[32], const uint8_t u_in[32]) {
    uint8_t e[32];
    for (int i = 0; i < 32; i++) e[i] = scalar[i];
    e[ 0] &= 248;
    e[31] &= 127;
    e[31] |= 64;

    uint8_t u_masked[32];
    for (int i = 0; i < 32; i++) u_masked[i] = u_in[i];
    u_masked[31] &= 0x7f;

    fe x1, x2, x3, z2, z3;
    fe A, B, C, D, AA, BB, E, DA, CB, t1, t2;
    fe_from_bytes(x1, u_masked);
    fe_one(x2);
    fe_zero(z2);
    fe_copy(x3, x1);
    fe_one(z3);

    uint64_t swap = 0;
    for (int t = 254; t >= 0; t--) {
        uint64_t k_t = (e[t >> 3] >> (t & 7)) & 1;
        swap ^= k_t;
        fe_cswap(x2, x3, swap);
        fe_cswap(z2, z3, swap);
        swap = k_t;

        // RFC 7748 ladder step
        fe_add(A,  x2, z2);            // A  = x2 + z2
        fe_sq (AA, A);                 // AA = A^2
        fe_sub(B,  x2, z2);            // B  = x2 - z2
        fe_sq (BB, B);                 // BB = B^2
        fe_sub(E,  AA, BB);            // E  = AA - BB
        fe_add(C,  x3, z3);            // C  = x3 + z3
        fe_sub(D,  x3, z3);            // D  = x3 - z3
        fe_mul(DA, D, A);              // DA = D * A
        fe_mul(CB, C, B);              // CB = C * B
        fe_add(t1, DA, CB);
        fe_sq (x3, t1);                // x3 = (DA + CB)^2
        fe_sub(t1, DA, CB);
        fe_sq (t2, t1);                // t2 = (DA - CB)^2
        fe_mul(z3, x1, t2);            // z3 = x1 * (DA - CB)^2
        fe_mul(x2, AA, BB);            // x2 = AA * BB
        fe_mul121665(t1, E);           // t1 = 121665 * E
        fe_add(t2, AA, t1);            // t2 = AA + 121665 * E
        fe_mul(z2, E, t2);             // z2 = E * (AA + 121665 * E)
    }
    fe_cswap(x2, x3, swap);
    fe_cswap(z2, z3, swap);

    fe_inv(z2, z2);
    fe_mul(x2, x2, z2);
    fe_to_bytes(out, x2);
}

void x25519_base(uint8_t out[32], const uint8_t scalar[32]) {
    static const uint8_t bp[32] = { 9 };
    x25519(out, scalar, bp);
}

// RFC 7748 §5.2 known-answer tests.
static int bytes_eq(const uint8_t* a, const uint8_t* b, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) if (a[i] != b[i]) return 0;
    return 1;
}

int x25519_selftest(void) {
    // KAT 1
    {
        const uint8_t k[32] = {
            0xa5,0x46,0xe3,0x6b,0xf0,0x52,0x7c,0x9d,
            0x3b,0x16,0x15,0x4b,0x82,0x46,0x5e,0xdd,
            0x62,0x14,0x4c,0x0a,0xc1,0xfc,0x5a,0x18,
            0x50,0x6a,0x22,0x44,0xba,0x44,0x9a,0xc4
        };
        const uint8_t u[32] = {
            0xe6,0xdb,0x68,0x67,0x58,0x30,0x30,0xdb,
            0x35,0x94,0xc1,0xa4,0x24,0xb1,0x5f,0x7c,
            0x72,0x66,0x24,0xec,0x26,0xb3,0x35,0x3b,
            0x10,0xa9,0x03,0xa6,0xd0,0xab,0x1c,0x4c
        };
        const uint8_t exp[32] = {
            0xc3,0xda,0x55,0x37,0x9d,0xe9,0xc6,0x90,
            0x8e,0x94,0xea,0x4d,0xf2,0x8d,0x08,0x4f,
            0x32,0xec,0xcf,0x03,0x49,0x1c,0x71,0xf7,
            0x54,0xb4,0x07,0x55,0x77,0xa2,0x85,0x52
        };
        uint8_t out[32];
        x25519(out, k, u);
        if (!bytes_eq(out, exp, 32)) return 0;
    }
    // KAT 2
    {
        const uint8_t k[32] = {
            0x4b,0x66,0xe9,0xd4,0xd1,0xb4,0x67,0x3c,
            0x5a,0xd2,0x26,0x91,0x95,0x7d,0x6a,0xf5,
            0xc1,0x1b,0x64,0x21,0xe0,0xea,0x01,0xd4,
            0x2c,0xa4,0x16,0x9e,0x79,0x18,0xba,0x0d
        };
        const uint8_t u[32] = {
            0xe5,0x21,0x0f,0x12,0x78,0x68,0x11,0xd3,
            0xf4,0xb7,0x95,0x9d,0x05,0x38,0xae,0x2c,
            0x31,0xdb,0xe7,0x10,0x6f,0xc0,0x3c,0x3e,
            0xfc,0x4c,0xd5,0x49,0xc7,0x15,0xa4,0x93
        };
        const uint8_t exp[32] = {
            0x95,0xcb,0xde,0x94,0x76,0xe8,0x90,0x7d,
            0x7a,0xad,0xe4,0x5c,0xb4,0xb8,0x73,0xf8,
            0x8b,0x59,0x5a,0x68,0x79,0x9f,0xa1,0x52,
            0xe6,0xf8,0xf7,0x64,0x7a,0xac,0x79,0x57
        };
        uint8_t out[32];
        x25519(out, k, u);
        if (!bytes_eq(out, exp, 32)) return 0;
    }
    return 1;
}
