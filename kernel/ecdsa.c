#include "ecdsa.h"
#include "bignum.h"
#include "sha256.h"

static const uint8_t P256_P[32] = {
    0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x01,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF
};
static const uint8_t P256_N[32] = {
    0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xBC,0xE6,0xFA,0xAD,0xA7,0x17,0x9E,0x84,
    0xF3,0xB9,0xCA,0xC2,0xFC,0x63,0x25,0x51
};
static const uint8_t P256_GX[32] = {
    0x6B,0x17,0xD1,0xF2,0xE1,0x2C,0x42,0x47,
    0xF8,0xBC,0xE6,0xE5,0x63,0xA4,0x40,0xF2,
    0x77,0x03,0x7D,0x81,0x2D,0xEB,0x33,0xA0,
    0xF4,0xA1,0x39,0x45,0xD8,0x98,0xC2,0x96
};
static const uint8_t P256_GY[32] = {
    0x4F,0xE3,0x42,0xE2,0xFE,0x1A,0x7F,0x9B,
    0x8E,0xE7,0xEB,0x4A,0x7C,0x0F,0x9E,0x16,
    0x2B,0xCE,0x33,0x57,0x6B,0x31,0x5E,0xCE,
    0xCB,0xB6,0x40,0x68,0x37,0xBF,0x51,0xF5
};

static void f_sub(bn_t r, const bn_t a, const bn_t b, const bn_t m) {
    bn_t neg_b;
    bn_copy(neg_b, m);

    int64_t borrow = 0;
    for (int i = 0; i < BN_LIMBS; i++) {
        int64_t d = (int64_t)neg_b[i] - b[i] - borrow;
        neg_b[i] = (uint32_t)d;
        borrow = (d < 0) ? 1 : 0;
    }
    bn_addmod(r, a, neg_b, m);
}

static void f_mul_u32(bn_t r, const bn_t a, uint32_t k, const bn_t m) {
    bn_t kk;
    bn_zero(kk);
    kk[0] = k;
    bn_mulmod(r, a, kk, m);
}

static void f_inv(bn_t r, const bn_t a, const bn_t m) {
    bn_t exp;
    bn_copy(exp, m);

    int64_t borrow = 2;
    for (int i = 0; i < BN_LIMBS && borrow; i++) {
        int64_t d = (int64_t)exp[i] - borrow;
        exp[i] = (uint32_t)d;
        borrow = (d < 0) ? 1 : 0;
    }
    bn_modexp(r, a, exp, m);
}

typedef struct { bn_t X, Y, Z; } ec_pt;

static int pt_is_inf(const ec_pt* p) { return bn_is_zero(p->Z); }
static void pt_set_inf(ec_pt* p) { bn_zero(p->X); bn_zero(p->Y); bn_zero(p->Z); }

static void pt_copy(ec_pt* r, const ec_pt* a) {
    bn_copy(r->X, a->X); bn_copy(r->Y, a->Y); bn_copy(r->Z, a->Z);
}

static void pt_dbl(ec_pt* r, const ec_pt* p) {
    if (pt_is_inf(p) || bn_is_zero(p->Y)) { pt_set_inf(r); return; }
    bn_t delta, gamma, beta, alpha, t1, t2, X3, Y3, Z3;
    bn_t P_bn;
    bn_from_be(P_bn, P256_P, 32);

    bn_mulmod(delta, p->Z, p->Z, P_bn);
    bn_mulmod(gamma, p->Y, p->Y, P_bn);
    bn_mulmod(beta,  p->X, gamma, P_bn);


    f_sub(t1, p->X, delta, P_bn);
    bn_addmod(t2, p->X, delta, P_bn);
    bn_mulmod(alpha, t1, t2, P_bn);
    f_mul_u32(alpha, alpha, 3, P_bn);


    bn_mulmod(X3, alpha, alpha, P_bn);
    bn_t eight_beta;
    f_mul_u32(eight_beta, beta, 8, P_bn);
    f_sub(X3, X3, eight_beta, P_bn);


    bn_addmod(t1, p->Y, p->Z, P_bn);
    bn_mulmod(Z3, t1, t1, P_bn);
    f_sub(Z3, Z3, gamma, P_bn);
    f_sub(Z3, Z3, delta, P_bn);


    bn_t four_beta;
    f_mul_u32(four_beta, beta, 4, P_bn);
    f_sub(t1, four_beta, X3, P_bn);
    bn_mulmod(Y3, alpha, t1, P_bn);
    bn_t gamma_sq;
    bn_mulmod(gamma_sq, gamma, gamma, P_bn);
    bn_t eight_gamma_sq;
    f_mul_u32(eight_gamma_sq, gamma_sq, 8, P_bn);
    f_sub(Y3, Y3, eight_gamma_sq, P_bn);

    bn_copy(r->X, X3); bn_copy(r->Y, Y3); bn_copy(r->Z, Z3);
}

static void pt_add(ec_pt* r, const ec_pt* p, const ec_pt* q) {
    if (pt_is_inf(p)) { pt_copy(r, q); return; }
    if (pt_is_inf(q)) { pt_copy(r, p); return; }
    bn_t P_bn;
    bn_from_be(P_bn, P256_P, 32);

    bn_t Z1Z1, Z2Z2, U1, U2, S1, S2, H, I, J, rr, V, X3, Y3, Z3, t;

    bn_mulmod(Z1Z1, p->Z, p->Z, P_bn);
    bn_mulmod(Z2Z2, q->Z, q->Z, P_bn);
    bn_mulmod(U1, p->X, Z2Z2, P_bn);
    bn_mulmod(U2, q->X, Z1Z1, P_bn);
    bn_mulmod(t, p->Z, Z1Z1, P_bn);
    bn_mulmod(S1, p->Y, q->Z, P_bn);
    bn_mulmod(S1, S1, Z2Z2, P_bn);
    bn_mulmod(S2, q->Y, p->Z, P_bn);
    bn_mulmod(S2, S2, Z1Z1, P_bn);

    f_sub(H, U2, U1, P_bn);
    f_sub(rr, S2, S1, P_bn);

    if (bn_is_zero(H)) {
        if (bn_is_zero(rr)) { pt_dbl(r, p); return; }
        pt_set_inf(r); return;
    }

    bn_mulmod(I, H, H, P_bn);
    f_mul_u32(I, I, 4, P_bn);
    bn_mulmod(J, H, I, P_bn);
    f_mul_u32(t, rr, 2, P_bn);
    bn_mulmod(V, U1, I, P_bn);


    bn_mulmod(X3, t, t, P_bn);
    f_sub(X3, X3, J, P_bn);
    bn_t twoV;
    f_mul_u32(twoV, V, 2, P_bn);
    f_sub(X3, X3, twoV, P_bn);


    f_sub(Y3, V, X3, P_bn);
    bn_mulmod(Y3, t, Y3, P_bn);
    bn_t S1J;
    bn_mulmod(S1J, S1, J, P_bn);
    bn_t twoS1J;
    f_mul_u32(twoS1J, S1J, 2, P_bn);
    f_sub(Y3, Y3, twoS1J, P_bn);


    bn_addmod(Z3, p->Z, q->Z, P_bn);
    bn_mulmod(Z3, Z3, Z3, P_bn);
    f_sub(Z3, Z3, Z1Z1, P_bn);
    f_sub(Z3, Z3, Z2Z2, P_bn);
    bn_mulmod(Z3, Z3, H, P_bn);

    bn_copy(r->X, X3); bn_copy(r->Y, Y3); bn_copy(r->Z, Z3);
}

static int pt_to_affine_x(bn_t out_x, const ec_pt* p, const bn_t P_bn) {
    if (pt_is_inf(p)) return 0;
    bn_t zinv, zinv2;
    f_inv(zinv, p->Z, P_bn);
    bn_mulmod(zinv2, zinv, zinv, P_bn);
    bn_mulmod(out_x, p->X, zinv2, P_bn);
    return 1;
}

static void pt_mul(ec_pt* r, const ec_pt* p, const bn_t k) {
    pt_set_inf(r);

    int top = -1;
    for (int i = BN_LIMBS * 32 - 1; i >= 0; i--) {
        if ((k[i >> 5] >> (i & 31)) & 1) { top = i; break; }
    }
    if (top < 0) return;
    for (int i = top; i >= 0; i--) {
        ec_pt t;
        pt_dbl(&t, r);
        pt_copy(r, &t);
        if ((k[i >> 5] >> (i & 31)) & 1) {
            pt_add(&t, r, p);
            pt_copy(r, &t);
        }
    }
}

static int parse_ecdsa_sig(const uint8_t* der, uint32_t len, bn_t r_out, bn_t s_out) {
    if (len < 4 || der[0] != 0x30) return 0;
    uint32_t p = 1;
    uint32_t seq_len;
    if (der[p] & 0x80) {
        uint32_t nl = der[p] & 0x7F; p++;
        if (nl == 0 || nl > 4 || p + nl > len) return 0;
        seq_len = 0;
        for (uint32_t i = 0; i < nl; i++) seq_len = (seq_len << 8) | der[p++];
    } else {
        seq_len = der[p++];
    }
    if (p + seq_len > len) return 0;
    uint32_t end = p + seq_len;
    for (int k = 0; k < 2; k++) {
        if (p >= end) return 0;
        if (der[p++] != 0x02) return 0;
        uint32_t il;
        if (der[p] & 0x80) {
            uint32_t nl = der[p++] & 0x7F;
            if (nl == 0 || p + nl > end) return 0;
            il = 0;
            for (uint32_t i = 0; i < nl; i++) il = (il << 8) | der[p++];
        } else il = der[p++];
        if (p + il > end) return 0;
        const uint8_t* iv = der + p;
        uint32_t actual_len = il;
        while (actual_len > 1 && iv[0] == 0x00) { iv++; actual_len--; }
        if (k == 0) bn_from_be(r_out, iv, actual_len);
        else        bn_from_be(s_out, iv, actual_len);
        p += il;
    }
    return 1;
}

int ecdsa_p256_sha256_verify(const uint8_t pubkey_x[32], const uint8_t pubkey_y[32],
                             const uint8_t* sig_der, uint32_t sig_der_len,
                             const uint8_t* msg, uint32_t msg_len) {
    bn_t r, s, n, p_mod;
    if (!parse_ecdsa_sig(sig_der, sig_der_len, r, s)) return 0;
    bn_from_be(n, P256_N, 32);
    bn_from_be(p_mod, P256_P, 32);

    if (bn_is_zero(r) || bn_is_zero(s)) return 0;
    if (bn_cmp(r, n) >= 0 || bn_cmp(s, n) >= 0) return 0;


    uint8_t hash[32];
    sha256(msg, msg_len, hash);
    bn_t e;
    bn_from_be(e, hash, 32);

    while (bn_cmp(e, n) >= 0) {

        int64_t borrow = 0;
        for (int i = 0; i < BN_LIMBS; i++) {
            int64_t d = (int64_t)e[i] - n[i] - borrow;
            e[i] = (uint32_t)d;
            borrow = (d < 0) ? 1 : 0;
        }
    }


    bn_t w;
    f_inv(w, s, n);


    bn_t u1, u2;
    bn_mulmod(u1, e, w, n);
    bn_mulmod(u2, r, w, n);


    ec_pt G, Q;
    bn_from_be(G.X, P256_GX, 32);
    bn_from_be(G.Y, P256_GY, 32);
    bn_zero(G.Z); G.Z[0] = 1;
    bn_from_be(Q.X, pubkey_x, 32);
    bn_from_be(Q.Y, pubkey_y, 32);
    bn_zero(Q.Z); Q.Z[0] = 1;


    ec_pt u1G, u2Q, R;
    pt_mul(&u1G, &G, u1);
    pt_mul(&u2Q, &Q, u2);
    pt_add(&R, &u1G, &u2Q);

    if (pt_is_inf(&R)) return 0;
    bn_t x_aff;
    if (!pt_to_affine_x(x_aff, &R, p_mod)) return 0;
    while (bn_cmp(x_aff, n) >= 0) {
        int64_t borrow = 0;
        for (int i = 0; i < BN_LIMBS; i++) {
            int64_t d = (int64_t)x_aff[i] - n[i] - borrow;
            x_aff[i] = (uint32_t)d;
            borrow = (d < 0) ? 1 : 0;
        }
    }
    return bn_cmp(x_aff, r) == 0;
}

int ecdsa_selftest(void) {

    const uint8_t Qx[32] = {
        0x60,0xFE,0xD4,0xBA,0x25,0x5A,0x9D,0x31,
        0xC9,0x61,0xEB,0x74,0xC6,0x35,0x6D,0x68,
        0xC0,0x49,0xB8,0x92,0x3B,0x61,0xFA,0x6C,
        0xE6,0x69,0x62,0x2E,0x60,0xF2,0x9F,0xB6
    };
    const uint8_t Qy[32] = {
        0x79,0x03,0xFE,0x10,0x08,0xB8,0xBC,0x99,
        0xA4,0x1A,0xE9,0xE9,0x56,0x28,0xBC,0x64,
        0xF2,0xF1,0xB2,0x0C,0x2D,0x7E,0x9F,0x51,
        0x77,0xA3,0xC2,0x94,0xD4,0x46,0x22,0x99
    };



    const uint8_t r_be[32] = {
        0xEF,0xD4,0x8B,0x2A,0xAC,0xB6,0xA8,0xFD,
        0x11,0x40,0xDD,0x9C,0xD4,0x5E,0x81,0xD6,
        0x9D,0x2C,0x87,0x7B,0x56,0xAA,0xF9,0x91,
        0xC3,0x4D,0x0E,0xA8,0x4E,0xAF,0x37,0x16
    };
    const uint8_t s_be[32] = {
        0xF7,0xCB,0x1C,0x94,0x2D,0x65,0x7C,0x41,
        0xD4,0x36,0xC7,0xA1,0xB6,0xE2,0x9F,0x65,
        0xF3,0xE9,0x00,0xDB,0xB9,0xAF,0xF4,0x06,
        0x4D,0xC4,0xAB,0x2F,0x84,0x3A,0xCD,0xA8
    };

    uint8_t sig[72];
    uint32_t p = 0;
    sig[p++] = 0x30; sig[p++] = 0x44;
    sig[p++] = 0x02; sig[p++] = 0x20;
    for (int i = 0; i < 32; i++) sig[p++] = r_be[i];
    sig[p++] = 0x02; sig[p++] = 0x20;
    for (int i = 0; i < 32; i++) sig[p++] = s_be[i];

    const char* msg = "sample";
    return ecdsa_p256_sha256_verify(Qx, Qy, sig, p,
                                    (const uint8_t*)msg, 6);
}
