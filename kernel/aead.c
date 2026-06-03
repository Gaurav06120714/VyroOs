#include "aead.h"
#include "chacha20.h"
#include "poly1305.h"

static void derive_poly_key(const uint8_t key[32], const uint8_t nonce[12],
                            uint8_t pkey[32]) {
    uint8_t zero[64] = {0};
    uint8_t block[64];
    chacha20_block(key, 0, nonce, block);
    for (int i = 0; i < 32; i++) pkey[i] = block[i] ^ zero[i];   // = block[i]
}

static void put_le64(uint8_t* p, uint64_t v) {
    for (int i = 0; i < 8; i++) p[i] = (uint8_t)(v >> (8 * i));
}

void aead_seal(const uint8_t key[32], const uint8_t nonce[12],
               const uint8_t* aad, uint32_t aad_len,
               const uint8_t* plain, uint32_t plain_len,
               uint8_t* cipher_out, uint8_t tag_out[16]) {
    uint8_t pkey[32];
    derive_poly_key(key, nonce, pkey);

    // Encrypt plaintext starting at counter=1
    chacha20_xor(key, 1, nonce, plain, cipher_out, plain_len);

    // MAC over: aad | pad16(aad) | cipher | pad16(cipher) | len(aad) | len(cipher)
    static uint8_t mac_in[16384];
    // Worst-case size = aad + 15 + plain + 15 + 16. Bail before overflow.
    if ((uint64_t)aad_len + 15 + (uint64_t)plain_len + 15 + 16 > sizeof(mac_in)) {
        for (int i = 0; i < 16; i++) tag_out[i] = 0;     // poisoned tag
        return;
    }
    uint32_t mp = 0;
    for (uint32_t i = 0; i < aad_len; i++) mac_in[mp++] = aad[i];
    while (mp % 16) mac_in[mp++] = 0;
    for (uint32_t i = 0; i < plain_len; i++) mac_in[mp++] = cipher_out[i];
    while (mp % 16) mac_in[mp++] = 0;
    put_le64(mac_in + mp, aad_len);   mp += 8;
    put_le64(mac_in + mp, plain_len); mp += 8;

    poly1305_mac(tag_out, mac_in, mp, pkey);
}

int aead_open(const uint8_t key[32], const uint8_t nonce[12],
              const uint8_t* aad, uint32_t aad_len,
              const uint8_t* cipher, uint32_t cipher_len,
              const uint8_t tag[16], uint8_t* plain_out) {
    uint8_t pkey[32];
    derive_poly_key(key, nonce, pkey);

    static uint8_t mac_in[16384];
    if ((uint64_t)aad_len + 15 + (uint64_t)cipher_len + 15 + 16 > sizeof(mac_in)) return 0;
    uint32_t mp = 0;
    for (uint32_t i = 0; i < aad_len; i++) mac_in[mp++] = aad[i];
    while (mp % 16) mac_in[mp++] = 0;
    for (uint32_t i = 0; i < cipher_len; i++) mac_in[mp++] = cipher[i];
    while (mp % 16) mac_in[mp++] = 0;
    put_le64(mac_in + mp, aad_len);    mp += 8;
    put_le64(mac_in + mp, cipher_len); mp += 8;

    uint8_t expected[16];
    poly1305_mac(expected, mac_in, mp, pkey);

    // Constant-time tag compare
    uint8_t diff = 0;
    for (int i = 0; i < 16; i++) diff |= expected[i] ^ tag[i];
    if (diff) return 0;

    chacha20_xor(key, 1, nonce, cipher, plain_out, cipher_len);
    return 1;
}

// ─────────────────────────────────────────────────────────────────────
// RFC 8439 test vectors (§2.6.2, §2.5.2, §2.8.2)
// ─────────────────────────────────────────────────────────────────────

static int bytes_eq(const uint8_t* a, const uint8_t* b, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) if (a[i] != b[i]) return 0;
    return 1;
}

int aead_selftest(void) {
    // §2.4.2: ChaCha20 stream cipher test vector — encrypt the famous Sunscreen
    // message and check the first 16 ciphertext bytes.
    {
        uint8_t key[32];
        for (int i = 0; i < 32; i++) key[i] = (uint8_t)i;
        uint8_t nonce[12] = { 0,0,0,0, 0,0,0,0x4a, 0,0,0,0 };
        const char* pt =
            "Ladies and Gentlemen of the class of '99: If I could offer you "
            "only one tip for the future, sunscreen would be it.";
        uint8_t ct[114];
        uint32_t n = 0;
        while (pt[n]) n++;
        chacha20_xor(key, 1, nonce, (const uint8_t*)pt, ct, n);
        // First 16 bytes of expected ciphertext from RFC 8439 §2.4.2
        const uint8_t exp[16] = {
            0x6e,0x2e,0x35,0x9a,0x25,0x68,0xf9,0x80,
            0x41,0xba,0x07,0x28,0xdd,0x0d,0x69,0x81
        };
        if (!bytes_eq(ct, exp, 16)) return 0;
    }

    // §2.5.2: Poly1305 MAC test vector
    {
        uint8_t key[32] = {
            0x85,0xd6,0xbe,0x78,0x57,0x55,0x6d,0x33,
            0x7f,0x44,0x52,0xfe,0x42,0xd5,0x06,0xa8,
            0x01,0x03,0x80,0x8a,0xfb,0x0d,0xb2,0xfd,
            0x4a,0xbf,0xf6,0xaf,0x41,0x49,0xf5,0x1b
        };
        const char* msg = "Cryptographic Forum Research Group";
        uint8_t tag[16];
        uint32_t n = 0;
        while (msg[n]) n++;
        poly1305_mac(tag, (const uint8_t*)msg, n, key);
        const uint8_t exp[16] = {
            0xa8,0x06,0x1d,0xc1,0x30,0x51,0x36,0xc6,
            0xc2,0x2b,0x8b,0xaf,0x0c,0x01,0x27,0xa9
        };
        if (!bytes_eq(tag, exp, 16)) return 0;
    }

    // §2.8.2: AEAD seal round-trip + tag check
    {
        const uint8_t key[32] = {
            0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,
            0x88,0x89,0x8a,0x8b,0x8c,0x8d,0x8e,0x8f,
            0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,
            0x98,0x99,0x9a,0x9b,0x9c,0x9d,0x9e,0x9f
        };
        const uint8_t nonce[12] = {
            0x07,0x00,0x00,0x00,
            0x40,0x41,0x42,0x43,0x44,0x45,0x46,0x47
        };
        const uint8_t aad[12] = {
            0x50,0x51,0x52,0x53,0xc0,0xc1,0xc2,0xc3,
            0xc4,0xc5,0xc6,0xc7
        };
        const char* pt =
            "Ladies and Gentlemen of the class of '99: If I could offer you "
            "only one tip for the future, sunscreen would be it.";
        uint32_t pn = 0; while (pt[pn]) pn++;

        uint8_t ct[114];
        uint8_t tag[16];
        aead_seal(key, nonce, aad, 12, (const uint8_t*)pt, pn, ct, tag);

        const uint8_t exp_tag[16] = {
            0x1a,0xe1,0x0b,0x59,0x4f,0x09,0xe2,0x6a,
            0x7e,0x90,0x2e,0xcb,0xd0,0x60,0x06,0x91
        };
        if (!bytes_eq(tag, exp_tag, 16)) return 0;

        uint8_t pt2[114];
        if (!aead_open(key, nonce, aad, 12, ct, pn, tag, pt2)) return 0;
        if (!bytes_eq(pt2, (const uint8_t*)pt, pn)) return 0;
    }

    return 1;
}
