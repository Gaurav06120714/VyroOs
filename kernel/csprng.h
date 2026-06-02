#ifndef CSPRNG_H
#define CSPRNG_H

#include "../include/types.h"

// Cryptographically-stronger pseudo-random generator.
// Construction: ChaCha20 keystream, key = SHA-256(entropy_pool),
// counter advances per 64-byte block. Entropy pool is reseeded
// from RDRAND (when available), RDTSC, timer ticks, and previous
// output. Not certified, but materially stronger than the
// timer-only sketch v3.11 was using.

void csprng_init(void);

// Fill `out` with `n` random bytes.
void csprng_bytes(uint8_t* out, uint32_t n);

// Stir additional entropy into the pool.
void csprng_reseed(const uint8_t* in, uint32_t n);

#endif
