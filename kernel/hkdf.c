#include "hkdf.h"
#include "sha256.h"

#define BLOCK_SIZE 64
#define DIGEST     32

void hmac_sha256(const uint8_t* key, uint32_t key_len,
                 const uint8_t* msg, uint32_t msg_len,
                 uint8_t out[32]) {
    uint8_t k[BLOCK_SIZE];
    for (int i = 0; i < BLOCK_SIZE; i++) k[i] = 0;

    if (key_len > BLOCK_SIZE) {
        sha256(key, key_len, k);                   // first 32 bytes; rest already zero
    } else {
        for (uint32_t i = 0; i < key_len; i++) k[i] = key[i];
    }

    uint8_t ipad[BLOCK_SIZE], opad[BLOCK_SIZE];
    for (int i = 0; i < BLOCK_SIZE; i++) {
        ipad[i] = k[i] ^ 0x36;
        opad[i] = k[i] ^ 0x5c;
    }

    // inner = SHA256(ipad || msg)
    static uint8_t buf[16 * 1024];                 // capped, see below
    if (BLOCK_SIZE + msg_len > sizeof(buf)) {
        // Defensive cap. TLS messages we care about are well under 16 KB.
        return;
    }
    for (int i = 0; i < BLOCK_SIZE; i++) buf[i] = ipad[i];
    for (uint32_t i = 0; i < msg_len; i++) buf[BLOCK_SIZE + i] = msg[i];
    uint8_t inner[DIGEST];
    sha256(buf, BLOCK_SIZE + msg_len, inner);

    // outer = SHA256(opad || inner)
    uint8_t outer_in[BLOCK_SIZE + DIGEST];
    for (int i = 0; i < BLOCK_SIZE; i++) outer_in[i] = opad[i];
    for (int i = 0; i < DIGEST; i++)     outer_in[BLOCK_SIZE + i] = inner[i];
    sha256(outer_in, BLOCK_SIZE + DIGEST, out);
}

void hkdf_extract(const uint8_t* salt, uint32_t salt_len,
                  const uint8_t* ikm,  uint32_t ikm_len,
                  uint8_t prk[32]) {
    uint8_t zero_salt[32];
    if (!salt || salt_len == 0) {
        for (int i = 0; i < 32; i++) zero_salt[i] = 0;
        salt = zero_salt;
        salt_len = 32;
    }
    hmac_sha256(salt, salt_len, ikm, ikm_len, prk);
}

void hkdf_expand(const uint8_t prk[32],
                 const uint8_t* info, uint32_t info_len,
                 uint8_t* out, uint32_t out_len) {
    uint8_t t[DIGEST];
    uint8_t prev[DIGEST];
    uint32_t off = 0;
    uint8_t counter = 1;

    // First block: T(1) = HMAC(PRK, info || 0x01)
    static uint8_t hmac_in[8192];
    while (off < out_len) {
        uint32_t pos = 0;
        if (counter > 1) {
            for (int i = 0; i < DIGEST; i++) hmac_in[pos++] = prev[i];
        }
        for (uint32_t i = 0; i < info_len; i++) hmac_in[pos++] = info[i];
        hmac_in[pos++] = counter;
        hmac_sha256(prk, 32, hmac_in, pos, t);

        uint32_t want = out_len - off;
        if (want > DIGEST) want = DIGEST;
        for (uint32_t i = 0; i < want; i++) out[off + i] = t[i];
        for (int i = 0; i < DIGEST; i++) prev[i] = t[i];
        off += want;
        counter++;
    }
}

static uint32_t mini_strlen(const char* s) {
    uint32_t n = 0; while (s[n]) n++; return n;
}

void tls13_hkdf_expand_label(const uint8_t prk[32],
                             const char* label_suffix,
                             const uint8_t* ctx, uint32_t ctx_len,
                             uint8_t* out, uint32_t out_len) {
    // label = "tls13 " || label_suffix
    const char prefix[] = "tls13 ";
    uint32_t suffix_len = mini_strlen(label_suffix);
    uint32_t label_len  = 6 + suffix_len;     // strlen("tls13 ") == 6

    // HkdfLabel = length(2) || label_len(1) || label || ctx_len(1) || ctx
    uint8_t info[2 + 1 + 255 + 1 + 255];
    uint32_t p = 0;
    info[p++] = (uint8_t)(out_len >> 8);
    info[p++] = (uint8_t)(out_len & 0xFF);
    info[p++] = (uint8_t)label_len;
    for (uint32_t i = 0; i < 6;          i++) info[p++] = (uint8_t)prefix[i];
    for (uint32_t i = 0; i < suffix_len; i++) info[p++] = (uint8_t)label_suffix[i];
    info[p++] = (uint8_t)ctx_len;
    for (uint32_t i = 0; i < ctx_len;    i++) info[p++] = ctx[i];

    hkdf_expand(prk, info, p, out, out_len);
}

void tls13_derive_secret(const uint8_t prk[32],
                         const char* label_suffix,
                         const uint8_t* messages, uint32_t messages_len,
                         uint8_t out[32]) {
    uint8_t mhash[32];
    sha256(messages, messages_len, mhash);
    tls13_hkdf_expand_label(prk, label_suffix, mhash, 32, out, 32);
}

// ─────────────────────────────────────────────────────────────────────
// Selftests against RFC 4231 (HMAC) and RFC 5869 (HKDF) test vectors.
// ─────────────────────────────────────────────────────────────────────

static int bytes_eq(const uint8_t* a, const uint8_t* b, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) if (a[i] != b[i]) return 0;
    return 1;
}

int hkdf_selftest(void) {
    // RFC 4231 §4.2 Test Case 1: key=20 × 0x0b, data="Hi There"
    {
        uint8_t key[20]; for (int i = 0; i < 20; i++) key[i] = 0x0b;
        const uint8_t msg[] = { 'H','i',' ','T','h','e','r','e' };
        uint8_t out[32];
        hmac_sha256(key, 20, msg, 8, out);
        const uint8_t exp[32] = {
            0xb0,0x34,0x4c,0x61,0xd8,0xdb,0x38,0x53,
            0x5c,0xa8,0xaf,0xce,0xaf,0x0b,0xf1,0x2b,
            0x88,0x1d,0xc2,0x00,0xc9,0x83,0x3d,0xa7,
            0x26,0xe9,0x37,0x6c,0x2e,0x32,0xcf,0xf7
        };
        if (!bytes_eq(out, exp, 32)) return 0;
    }

    // RFC 5869 §A.1 Test Case 1: HKDF-SHA256
    //   IKM  = 22 × 0x0b
    //   salt = 0x000102030405060708090a0b0c
    //   info = 0xf0f1f2f3f4f5f6f7f8f9
    //   L    = 42
    //   PRK  = 077709362c2e32df0ddc3f0dc47bba63...90cd1f3f
    //   OKM  = 3cb25f25faacd57a90434f64d0362f2a...34007208d5b887185865
    {
        uint8_t ikm[22]; for (int i = 0; i < 22; i++) ikm[i] = 0x0b;
        const uint8_t salt[13] = {
            0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
            0x08,0x09,0x0a,0x0b,0x0c
        };
        const uint8_t info[10] = {
            0xf0,0xf1,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf8,0xf9
        };
        uint8_t prk[32];
        hkdf_extract(salt, 13, ikm, 22, prk);
        const uint8_t exp_prk[32] = {
            0x07,0x77,0x09,0x36,0x2c,0x2e,0x32,0xdf,
            0x0d,0xdc,0x3f,0x0d,0xc4,0x7b,0xba,0x63,
            0x90,0xb6,0xc7,0x3b,0xb5,0x0f,0x9c,0x31,
            0x22,0xec,0x84,0x4a,0xd7,0xc2,0xb3,0xe5
        };
        if (!bytes_eq(prk, exp_prk, 32)) return 0;

        uint8_t okm[42];
        hkdf_expand(prk, info, 10, okm, 42);
        const uint8_t exp_okm[42] = {
            0x3c,0xb2,0x5f,0x25,0xfa,0xac,0xd5,0x7a,
            0x90,0x43,0x4f,0x64,0xd0,0x36,0x2f,0x2a,
            0x2d,0x2d,0x0a,0x90,0xcf,0x1a,0x5a,0x4c,
            0x5d,0xb0,0x2d,0x56,0xec,0xc4,0xc5,0xbf,
            0x34,0x00,0x72,0x08,0xd5,0xb8,0x87,0x18,
            0x58,0x65
        };
        if (!bytes_eq(okm, exp_okm, 42)) return 0;
    }
    return 1;
}
