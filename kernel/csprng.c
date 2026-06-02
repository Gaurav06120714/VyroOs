#include "csprng.h"
#include "sha256.h"
#include "chacha20.h"
#include "../drivers/timer.h"

static uint8_t  pool[64];           // raw entropy bytes
static uint32_t pool_pos = 0;
static uint8_t  key[32];            // ChaCha20 key derived from pool
static uint8_t  nonce[12];
static uint32_t counter;
static int      initialised = 0;

static inline uint64_t rdtsc(void) {
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

// Try RDRAND. Returns 1 on success.
static int rdrand64(uint64_t* out) {
    uint64_t v;
    uint8_t ok = 0;
    __asm__ volatile(
        ".byte 0x48, 0x0F, 0xC7, 0xF0\n"   // rdrand %rax
        "setc %1\n"
        : "=a"(v), "=qm"(ok)
        :
        : "cc");
    if (ok) { *out = v; return 1; }
    return 0;
}

static void stir_pool_byte(uint8_t b) {
    pool[pool_pos % sizeof(pool)] ^= b;
    pool_pos++;
}

static void rekey(void) {
    // key = SHA-256(pool || rdtsc || prior key)
    uint8_t in[64 + 8 + 32];
    for (uint32_t i = 0; i < sizeof(pool); i++) in[i] = pool[i];
    uint64_t t = rdtsc();
    for (int i = 0; i < 8; i++) in[64 + i] = (uint8_t)(t >> (8 * i));
    for (int i = 0; i < 32; i++) in[72 + i] = key[i];
    sha256(in, sizeof(in), key);
    // nonce = first 12 bytes of SHA-256(key || rdtsc)
    uint8_t nin[40];
    for (int i = 0; i < 32; i++) nin[i] = key[i];
    uint64_t t2 = rdtsc();
    for (int i = 0; i < 8; i++) nin[32 + i] = (uint8_t)(t2 >> (8 * i));
    uint8_t nh[32];
    sha256(nin, sizeof(nin), nh);
    for (int i = 0; i < 12; i++) nonce[i] = nh[i];
    counter = 0;
}

void csprng_reseed(const uint8_t* in, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) stir_pool_byte(in[i]);
    rekey();
}

void csprng_init(void) {
    if (initialised) return;
    for (uint32_t i = 0; i < sizeof(pool); i++) pool[i] = 0;
    // Seed from RDRAND if available
    for (int i = 0; i < 8; i++) {
        uint64_t r;
        if (rdrand64(&r)) {
            for (int j = 0; j < 8; j++) stir_pool_byte((uint8_t)(r >> (8 * j)));
        }
    }
    // Always mix in timer / rdtsc
    uint64_t a = timer_ticks();
    uint64_t b = rdtsc();
    uint64_t c = timer_uptime_ms();
    for (int i = 0; i < 8; i++) {
        stir_pool_byte((uint8_t)(a >> (8 * i)));
        stir_pool_byte((uint8_t)(b >> (8 * i)));
        stir_pool_byte((uint8_t)(c >> (8 * i)));
    }
    rekey();
    initialised = 1;
}

void csprng_bytes(uint8_t* out, uint32_t n) {
    if (!initialised) csprng_init();
    while (n > 0) {
        uint8_t block[64];
        chacha20_block(key, counter++, nonce, block);
        uint32_t take = n < 64 ? n : 64;
        for (uint32_t i = 0; i < take; i++) out[i] = block[i];
        out += take;
        n   -= take;
        // After every 1 KB stir in a fresh rdtsc sample
        if ((counter & 0x0F) == 0) {
            uint64_t r = rdtsc();
            for (int i = 0; i < 8; i++) stir_pool_byte((uint8_t)(r >> (8 * i)));
        }
    }
}
