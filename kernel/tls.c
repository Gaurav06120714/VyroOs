#include "tls.h"
#include "hkdf.h"
#include "x25519.h"
#include "sha256.h"

static uint32_t mini_strlen(const char* s) {
    uint32_t n = 0; while (s && s[n]) n++; return n;
}

// ─────────────────────────────────────────────────────────────────────
// ClientHello builder
// ─────────────────────────────────────────────────────────────────────

static void put16(uint8_t* p, uint16_t v) { p[0] = v >> 8; p[1] = v & 0xff; }
static void put24(uint8_t* p, uint32_t v) { p[0] = (v >> 16) & 0xff; p[1] = (v >> 8) & 0xff; p[2] = v & 0xff; }

int tls_build_client_hello(uint8_t* out, uint32_t out_max,
                           const uint8_t client_random[32],
                           const uint8_t session_id[32],
                           const uint8_t client_pub_x25519[32],
                           const char* hostname) {
    if (out_max < 256) return -1;
    uint32_t p = 0;

    // Reserve record header (5) and handshake header (4) — fill later
    if (out_max < 9) return -1;
    p = 9;

    // ─── ClientHello body ───
    // legacy_version
    out[p++] = 0x03; out[p++] = 0x03;
    // random[32]
    for (int i = 0; i < 32; i++) out[p++] = client_random[i];
    // legacy_session_id<0..32>
    out[p++] = 32;
    for (int i = 0; i < 32; i++) out[p++] = session_id[i];
    // cipher_suites<2..2^16-2>
    out[p++] = 0x00; out[p++] = 0x02;
    put16(out + p, TLS_CIPHER_CHACHA20_POLY1305_SHA256); p += 2;
    // legacy_compression_methods<1..2^8-1>
    out[p++] = 0x01; out[p++] = 0x00;

    // ─── extensions<8..2^16-1> ───
    uint32_t ext_len_off = p;
    p += 2;
    uint32_t ext_start = p;

    // supported_versions (43 = 0x002b): list = [0x0304]
    put16(out + p, 0x002b); p += 2;
    put16(out + p, 3);      p += 2;       // ext length
    out[p++] = 2;                         // list length
    put16(out + p, 0x0304); p += 2;       // TLS 1.3

    // supported_groups (10 = 0x000a): list = [x25519]
    put16(out + p, 0x000a); p += 2;
    put16(out + p, 4);      p += 2;
    put16(out + p, 2);      p += 2;       // list length
    put16(out + p, TLS_GROUP_X25519); p += 2;

    // signature_algorithms (13 = 0x000d):
    //   ecdsa_secp256r1_sha256 = 0x0403
    //   rsa_pss_rsae_sha256    = 0x0804
    //   rsa_pkcs1_sha256       = 0x0401
    put16(out + p, 0x000d); p += 2;
    put16(out + p, 8);      p += 2;
    put16(out + p, 6);      p += 2;
    put16(out + p, 0x0403); p += 2;
    put16(out + p, 0x0804); p += 2;
    put16(out + p, 0x0401); p += 2;

    // key_share (51 = 0x0033): x25519 32-byte pubkey
    put16(out + p, 0x0033); p += 2;
    put16(out + p, 4 + 32 + 2); p += 2;          // ext length = client_shares list length(2)+entry(2+2+32)
    put16(out + p, 32 + 4);     p += 2;          // list length
    put16(out + p, TLS_GROUP_X25519); p += 2;
    put16(out + p, 32); p += 2;
    for (int i = 0; i < 32; i++) out[p++] = client_pub_x25519[i];

    // server_name (0 = 0x0000) SNI, if hostname provided
    uint32_t hn = mini_strlen(hostname);
    if (hn > 0 && hn < 256) {
        put16(out + p, 0x0000); p += 2;
        uint32_t name_len = hn;
        uint32_t entry_len = 1 + 2 + name_len;
        uint32_t list_len = entry_len;
        uint32_t ext_data_len = 2 + list_len;
        put16(out + p, (uint16_t)ext_data_len); p += 2;
        put16(out + p, (uint16_t)list_len);     p += 2;
        out[p++] = 0;                                       // host_name
        put16(out + p, (uint16_t)name_len);     p += 2;
        for (uint32_t i = 0; i < name_len; i++) out[p++] = (uint8_t)hostname[i];
    }

    uint32_t ext_data_total = p - ext_start;
    put16(out + ext_len_off, (uint16_t)ext_data_total);

    // ─── Handshake header (4 bytes at offset 5) ───
    uint32_t hs_body_len = p - 9;
    out[5] = TLS_HS_CLIENT_HELLO;
    put24(out + 6, hs_body_len);

    // ─── Record header (5 bytes at offset 0) ───
    uint32_t record_body_len = p - 5;
    out[0] = TLS_RECORD_HANDSHAKE;
    out[1] = 0x03; out[2] = 0x03;            // legacy_record_version
    put16(out + 3, (uint16_t)record_body_len);

    return (int)p;
}

// ─────────────────────────────────────────────────────────────────────
// ServerHello parser — extracts the server's key_share X25519 pubkey.
// Input is the handshake body (no record header, no handshake header).
// ─────────────────────────────────────────────────────────────────────
int tls_parse_server_hello(const uint8_t* b, uint32_t n, uint8_t server_pub[32]) {
    if (n < 38) return 0;
    uint32_t p = 0;
    if (b[p++] != 0x03 || b[p++] != 0x03) return 0;          // legacy_version
    p += 32;                                                  // random
    if (p >= n) return 0;
    uint8_t sid_len = b[p++];
    if (p + sid_len > n) return 0;
    p += sid_len;
    if (p + 2 > n) return 0;
    uint16_t cipher = ((uint16_t)b[p] << 8) | b[p + 1]; p += 2;
    if (cipher != TLS_CIPHER_CHACHA20_POLY1305_SHA256) return 0;
    if (p + 1 > n) return 0;
    if (b[p++] != 0x00) return 0;                            // legacy_compression_method
    if (p + 2 > n) return 0;
    uint16_t ext_total = ((uint16_t)b[p] << 8) | b[p + 1]; p += 2;
    if (p + ext_total > n) return 0;

    uint32_t end = p + ext_total;
    while (p + 4 <= end) {
        uint16_t ext_type = ((uint16_t)b[p] << 8) | b[p + 1]; p += 2;
        uint16_t ext_len  = ((uint16_t)b[p] << 8) | b[p + 1]; p += 2;
        if (p + ext_len > end) return 0;
        if (ext_type == 0x0033) {                            // key_share
            // Format: group(2) | key_len(2) | key bytes
            if (ext_len < 4) return 0;
            uint16_t group = ((uint16_t)b[p] << 8) | b[p + 1];
            uint16_t klen  = ((uint16_t)b[p + 2] << 8) | b[p + 3];
            if (group != TLS_GROUP_X25519 || klen != 32 || ext_len < 4 + 32) return 0;
            for (int i = 0; i < 32; i++) server_pub[i] = b[p + 4 + i];
            return 1;
        }
        p += ext_len;
    }
    return 0;
}

// ─────────────────────────────────────────────────────────────────────
// TLS 1.3 key schedule (RFC 8446 §7.1).
// ─────────────────────────────────────────────────────────────────────
void tls_derive_handshake_keys(const uint8_t shared_secret[32],
                               const uint8_t ch_sh_hash[32],
                               tls_handshake_keys_t* out) {
    uint8_t zero32[32];
    for (int i = 0; i < 32; i++) zero32[i] = 0;

    // early_secret = HKDF-Extract(salt=0, IKM=0)
    uint8_t early_secret[32];
    hkdf_extract(zero32, 32, zero32, 32, early_secret);

    // derived_early = Derive-Secret(early_secret, "derived", "")
    uint8_t derived_early[32];
    tls13_derive_secret(early_secret, "derived", (const uint8_t*)"", 0, derived_early);

    // handshake_secret = HKDF-Extract(derived_early, shared_secret)
    hkdf_extract(derived_early, 32, shared_secret, 32, out->handshake_secret);

    // c hs traffic / s hs traffic
    tls13_hkdf_expand_label(out->handshake_secret, "c hs traffic",
                            ch_sh_hash, 32,
                            out->client_hs_traffic_secret, 32);
    tls13_hkdf_expand_label(out->handshake_secret, "s hs traffic",
                            ch_sh_hash, 32,
                            out->server_hs_traffic_secret, 32);

    // per-direction key/iv
    tls13_hkdf_expand_label(out->client_hs_traffic_secret, "key",
                            (const uint8_t*)"", 0, out->client_key, 32);
    tls13_hkdf_expand_label(out->client_hs_traffic_secret, "iv",
                            (const uint8_t*)"", 0, out->client_iv, 12);
    tls13_hkdf_expand_label(out->server_hs_traffic_secret, "key",
                            (const uint8_t*)"", 0, out->server_key, 32);
    tls13_hkdf_expand_label(out->server_hs_traffic_secret, "iv",
                            (const uint8_t*)"", 0, out->server_iv, 12);
}

// ─────────────────────────────────────────────────────────────────────
// Selftest — RFC 8448 §3 (Simple 1-RTT Handshake) known values.
// ─────────────────────────────────────────────────────────────────────
static int bytes_eq(const uint8_t* a, const uint8_t* b, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) if (a[i] != b[i]) return 0;
    return 1;
}

int tls_selftest(void) {
    // RFC 8448 §3 vectors
    const uint8_t client_priv[32] = {
        0x49,0xaf,0x42,0xba,0x7f,0x79,0x94,0x85,
        0x2d,0x71,0x3e,0xf2,0x78,0x4b,0xcb,0xca,
        0xa7,0x91,0x1d,0xe2,0x6a,0xdc,0x56,0x42,
        0xcb,0x63,0x45,0x40,0xe7,0xea,0x50,0x05
    };
    const uint8_t expected_client_pub[32] = {
        0x99,0x38,0x1d,0xe5,0x60,0xe4,0xbd,0x43,
        0xd2,0x3d,0x8e,0x43,0x5a,0x7d,0xba,0xfe,
        0xb3,0xc0,0x6e,0x51,0xc1,0x3c,0xae,0x4d,
        0x54,0x13,0x69,0x1e,0x52,0x9a,0xaf,0x2c
    };
    const uint8_t server_priv[32] = {
        0xb1,0x58,0x0e,0xea,0xdf,0x6d,0xd5,0x89,
        0xb8,0xef,0x4f,0x2d,0x56,0x52,0x57,0x8c,
        0xc8,0x10,0xe9,0x98,0x01,0x91,0xec,0x8d,
        0x05,0x83,0x08,0xce,0xa2,0x16,0xa2,0x1e
    };
    const uint8_t expected_shared[32] = {
        0x8b,0xd4,0x05,0x4f,0xb5,0x5b,0x9d,0x63,
        0xfd,0xfb,0xac,0xf9,0xf0,0x4b,0x9f,0x0d,
        0x35,0xe6,0xd6,0x3f,0x53,0x75,0x63,0xef,
        0xd4,0x62,0x72,0x90,0x0f,0x89,0x49,0x2d
    };
    const uint8_t expected_handshake_secret[32] = {
        0x1d,0xc8,0x26,0xe9,0x36,0x06,0xaa,0x6f,
        0xdc,0x0a,0xad,0xc1,0x2f,0x74,0x1b,0x01,
        0x04,0x6a,0xa6,0xb9,0x9f,0x69,0x1e,0xd2,
        0x21,0xa9,0xf0,0xca,0x04,0x3f,0xbe,0xac
    };

    // Verify client_pub from client_priv (X25519 self-derive check).
    uint8_t cp[32], sp[32];
    x25519_base(cp, client_priv);
    if (!bytes_eq(cp, expected_client_pub, 32)) return 0;

    // server pub
    x25519_base(sp, server_priv);

    // shared
    uint8_t sh[32];
    x25519(sh, client_priv, sp);
    if (!bytes_eq(sh, expected_shared, 32)) return 0;

    // Key schedule: feed the published transcript hash from RFC 8448 §3
    //   transcript-hash(CH||SH) = "860c06edc07858ee8e78f0e7428c58edd6b43f2ca3e6e95f02ed063cf0e1cad8"
    const uint8_t ch_sh_hash[32] = {
        0x86,0x0c,0x06,0xed,0xc0,0x78,0x58,0xee,
        0x8e,0x78,0xf0,0xe7,0x42,0x8c,0x58,0xed,
        0xd6,0xb4,0x3f,0x2c,0xa3,0xe6,0xe9,0x5f,
        0x02,0xed,0x06,0x3c,0xf0,0xe1,0xca,0xd8
    };
    tls_handshake_keys_t keys;
    tls_derive_handshake_keys(sh, ch_sh_hash, &keys);
    if (!bytes_eq(keys.handshake_secret, expected_handshake_secret, 32)) return 0;

    // ClientHello sanity: build one and verify the record/handshake headers
    // and a sampling of the bytes (cipher suite, x25519 pubkey).
    static uint8_t ch[512];
    uint8_t random[32];
    for (int i = 0; i < 32; i++) random[i] = (uint8_t)(0xA0 + i);
    uint8_t sid[32];
    for (int i = 0; i < 32; i++) sid[i] = (uint8_t)(0xB0 ^ i);
    int n = tls_build_client_hello(ch, sizeof(ch), random, sid, expected_client_pub, "vyro.test");
    if (n < 60) return 0;
    if (ch[0] != TLS_RECORD_HANDSHAKE) return 0;
    if (ch[5] != TLS_HS_CLIENT_HELLO)  return 0;
    // Round-trip: search for the pubkey bytes in the buffer; they must appear.
    int found = 0;
    for (int i = 0; i + 32 <= n && !found; i++) {
        if (bytes_eq(ch + i, expected_client_pub, 32)) found = 1;
    }
    if (!found) return 0;

    return 1;
}
