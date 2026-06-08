#include "chacha20.h"

static inline uint32_t rotl32(uint32_t x, unsigned n) {
    return (x << n) | (x >> (32 - n));
}

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

#define QR(a,b,c,d)                              \
    do {                                          \
        a += b; d ^= a; d = rotl32(d, 16);        \
        c += d; b ^= c; b = rotl32(b, 12);        \
        a += b; d ^= a; d = rotl32(d, 8);         \
        c += d; b ^= c; b = rotl32(b, 7);         \
    } while (0)

void chacha20_block(const uint8_t key[32], uint32_t counter,
                    const uint8_t nonce[12], uint8_t out[64]) {
    uint32_t s[16];
    s[ 0] = 0x61707865u;
    s[ 1] = 0x3320646eu;
    s[ 2] = 0x79622d32u;
    s[ 3] = 0x6b206574u;
    for (int i = 0; i < 8; i++) s[4 + i] = load32_le(key + i * 4);
    s[12] = counter;
    s[13] = load32_le(nonce + 0);
    s[14] = load32_le(nonce + 4);
    s[15] = load32_le(nonce + 8);

    uint32_t w[16];
    for (int i = 0; i < 16; i++) w[i] = s[i];

    for (int r = 0; r < 10; r++) {

        QR(w[0], w[4], w[ 8], w[12]);
        QR(w[1], w[5], w[ 9], w[13]);
        QR(w[2], w[6], w[10], w[14]);
        QR(w[3], w[7], w[11], w[15]);

        QR(w[0], w[5], w[10], w[15]);
        QR(w[1], w[6], w[11], w[12]);
        QR(w[2], w[7], w[ 8], w[13]);
        QR(w[3], w[4], w[ 9], w[14]);
    }

    for (int i = 0; i < 16; i++) store32_le(out + i * 4, w[i] + s[i]);
}

void chacha20_xor(const uint8_t key[32], uint32_t counter_start,
                  const uint8_t nonce[12],
                  const uint8_t* in, uint8_t* out, uint32_t len) {
    uint8_t block[64];
    uint32_t counter = counter_start;
    while (len > 0) {
        chacha20_block(key, counter++, nonce, block);
        uint32_t n = len < 64 ? len : 64;
        for (uint32_t i = 0; i < n; i++) out[i] = in[i] ^ block[i];
        in  += n;
        out += n;
        len -= n;
    }
}
