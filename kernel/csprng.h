#ifndef CSPRNG_H
#define CSPRNG_H

#include "../include/types.h"

void csprng_init(void);

void csprng_bytes(uint8_t* out, uint32_t n);

void csprng_reseed(const uint8_t* in, uint32_t n);

#endif
