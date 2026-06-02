#include "poly1305.h"

// Reference implementation in the style of poly1305-donna-32: 26-bit limbs,
// straightforward field multiplication mod 2^130 - 5.

static inline uint32_t load32_le(const uint8_t* p) {
    return  (uint32_t)p[0]        |
           ((uint32_t)p[1] <<  8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}
static inline void store32_le(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v       );
    p[1] = (uint8_t)(v >>  8 );
    p[2] = (uint8_t)(v >> 16 );
    p[3] = (uint8_t)(v >> 24 );
}

typedef struct {
    uint32_t r[5];          // clamped r as 5 × 26-bit
    uint32_t h[5];          // accumulator
    uint32_t pad[4];        // s as 4 × 32-bit (little-endian)
    uint8_t  buf[16];
    uint32_t buf_used;
    uint8_t  final;
} poly1305_ctx;

static void poly1305_init(poly1305_ctx* st, const uint8_t key[32]) {
    // Load r and clamp per RFC 8439
    uint32_t t0 = load32_le(key +  0);
    uint32_t t1 = load32_le(key +  4);
    uint32_t t2 = load32_le(key +  8);
    uint32_t t3 = load32_le(key + 12);
    st->r[0] = ( t0                    ) & 0x3ffffff;
    st->r[1] = ((t0 >> 26) | (t1 <<  6)) & 0x3ffff03;
    st->r[2] = ((t1 >> 20) | (t2 << 12)) & 0x3ffc0ff;
    st->r[3] = ((t2 >> 14) | (t3 << 18)) & 0x3f03fff;
    st->r[4] = ( t3 >>  8              ) & 0x00fffff;

    for (int i = 0; i < 5; i++) st->h[i] = 0;
    st->pad[0] = load32_le(key + 16);
    st->pad[1] = load32_le(key + 20);
    st->pad[2] = load32_le(key + 24);
    st->pad[3] = load32_le(key + 28);
    st->buf_used = 0;
    st->final    = 0;
}

static void poly1305_block(poly1305_ctx* st, const uint8_t* m) {
    const uint32_t hibit = st->final ? 0 : (1u << 24);

    uint32_t r0 = st->r[0], r1 = st->r[1], r2 = st->r[2], r3 = st->r[3], r4 = st->r[4];
    uint32_t s1 = r1 * 5, s2 = r2 * 5, s3 = r3 * 5, s4 = r4 * 5;
    uint32_t h0 = st->h[0], h1 = st->h[1], h2 = st->h[2], h3 = st->h[3], h4 = st->h[4];

    // Load 128-bit message block + 2^128 (or +0 for final partial) as 5 × 26-bit
    uint32_t t0 = load32_le(m +  0);
    uint32_t t1 = load32_le(m +  4);
    uint32_t t2 = load32_le(m +  8);
    uint32_t t3 = load32_le(m + 12);

    h0 += ( t0                    ) & 0x3ffffff;
    h1 += ((t0 >> 26) | (t1 <<  6)) & 0x3ffffff;
    h2 += ((t1 >> 20) | (t2 << 12)) & 0x3ffffff;
    h3 += ((t2 >> 14) | (t3 << 18)) & 0x3ffffff;
    h4 += ( t3 >>  8              ) | hibit;

    // h *= r mod (2^130 - 5)
    uint64_t d0 = (uint64_t)h0 * r0 + (uint64_t)h1 * s4 + (uint64_t)h2 * s3 + (uint64_t)h3 * s2 + (uint64_t)h4 * s1;
    uint64_t d1 = (uint64_t)h0 * r1 + (uint64_t)h1 * r0 + (uint64_t)h2 * s4 + (uint64_t)h3 * s3 + (uint64_t)h4 * s2;
    uint64_t d2 = (uint64_t)h0 * r2 + (uint64_t)h1 * r1 + (uint64_t)h2 * r0 + (uint64_t)h3 * s4 + (uint64_t)h4 * s3;
    uint64_t d3 = (uint64_t)h0 * r3 + (uint64_t)h1 * r2 + (uint64_t)h2 * r1 + (uint64_t)h3 * r0 + (uint64_t)h4 * s4;
    uint64_t d4 = (uint64_t)h0 * r4 + (uint64_t)h1 * r3 + (uint64_t)h2 * r2 + (uint64_t)h3 * r1 + (uint64_t)h4 * r0;

    // Carry propagate
    uint32_t c;
    c = (uint32_t)(d0 >> 26); h0 = (uint32_t)d0 & 0x3ffffff; d1 += c;
    c = (uint32_t)(d1 >> 26); h1 = (uint32_t)d1 & 0x3ffffff; d2 += c;
    c = (uint32_t)(d2 >> 26); h2 = (uint32_t)d2 & 0x3ffffff; d3 += c;
    c = (uint32_t)(d3 >> 26); h3 = (uint32_t)d3 & 0x3ffffff; d4 += c;
    c = (uint32_t)(d4 >> 26); h4 = (uint32_t)d4 & 0x3ffffff; h0 += c * 5;
    c = h0 >> 26;             h0 = h0 & 0x3ffffff;            h1 += c;

    st->h[0] = h0; st->h[1] = h1; st->h[2] = h2; st->h[3] = h3; st->h[4] = h4;
}

static void poly1305_update(poly1305_ctx* st, const uint8_t* m, uint32_t len) {
    if (st->buf_used) {
        uint32_t want = 16 - st->buf_used;
        if (want > len) want = len;
        for (uint32_t i = 0; i < want; i++) st->buf[st->buf_used + i] = m[i];
        st->buf_used += want;
        m += want; len -= want;
        if (st->buf_used == 16) {
            poly1305_block(st, st->buf);
            st->buf_used = 0;
        }
    }
    while (len >= 16) {
        poly1305_block(st, m);
        m += 16; len -= 16;
    }
    for (uint32_t i = 0; i < len; i++) st->buf[i] = m[i];
    st->buf_used = len;
}

static void poly1305_finish(poly1305_ctx* st, uint8_t out[16]) {
    if (st->buf_used) {
        st->buf[st->buf_used++] = 1;
        while (st->buf_used < 16) st->buf[st->buf_used++] = 0;
        st->final = 1;
        poly1305_block(st, st->buf);
    }

    uint32_t h0 = st->h[0], h1 = st->h[1], h2 = st->h[2], h3 = st->h[3], h4 = st->h[4];

    // Fully reduce h
    uint32_t c;
    c = h1 >> 26; h1 &= 0x3ffffff; h2 += c;
    c = h2 >> 26; h2 &= 0x3ffffff; h3 += c;
    c = h3 >> 26; h3 &= 0x3ffffff; h4 += c;
    c = h4 >> 26; h4 &= 0x3ffffff; h0 += c * 5;
    c = h0 >> 26; h0 &= 0x3ffffff; h1 += c;

    // Compute h + (-p) and select if no borrow
    uint32_t g0 = h0 + 5; c = g0 >> 26; g0 &= 0x3ffffff;
    uint32_t g1 = h1 + c; c = g1 >> 26; g1 &= 0x3ffffff;
    uint32_t g2 = h2 + c; c = g2 >> 26; g2 &= 0x3ffffff;
    uint32_t g3 = h3 + c; c = g3 >> 26; g3 &= 0x3ffffff;
    uint32_t g4 = h4 + c - (1u << 26);

    uint32_t mask = (g4 >> 31) - 1;             // 0xFFFFFFFF if g4 high bit clear
    g0 &= mask; g1 &= mask; g2 &= mask; g3 &= mask; g4 &= mask;
    mask = ~mask;
    h0 = (h0 & mask) | g0;
    h1 = (h1 & mask) | g1;
    h2 = (h2 & mask) | g2;
    h3 = (h3 & mask) | g3;
    h4 = (h4 & mask) | g4;

    // Repack 5 × 26-bit limbs into 4 × 32-bit
    uint32_t f0 =  h0        | (h1 << 26);
    uint32_t f1 = (h1 >>  6) | (h2 << 20);
    uint32_t f2 = (h2 >> 12) | (h3 << 14);
    uint32_t f3 = (h3 >> 18) | (h4 <<  8);

    // Add s (pad) mod 2^128
    uint64_t s0 = (uint64_t)f0 + st->pad[0]; f0 = (uint32_t)s0;
    uint64_t s1 = (uint64_t)f1 + st->pad[1] + (s0 >> 32); f1 = (uint32_t)s1;
    uint64_t s2 = (uint64_t)f2 + st->pad[2] + (s1 >> 32); f2 = (uint32_t)s2;
    uint64_t s3v = (uint64_t)f3 + st->pad[3] + (s2 >> 32); f3 = (uint32_t)s3v;

    store32_le(out +  0, f0);
    store32_le(out +  4, f1);
    store32_le(out +  8, f2);
    store32_le(out + 12, f3);
}

void poly1305_mac(uint8_t out[16], const uint8_t* msg, uint32_t len,
                  const uint8_t key[32]) {
    poly1305_ctx st;
    poly1305_init(&st, key);
    poly1305_update(&st, msg, len);
    poly1305_finish(&st, out);
}
