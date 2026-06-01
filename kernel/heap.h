#ifndef HEAP_H
#define HEAP_H

#include "../include/types.h"

#define HEAP_START  0x500000    // Heap begins at 5MB
#define HEAP_SIZE   0x800000    // 8MB heap

void  heap_init();
void* kmalloc(size_t size);
void  kfree(void* ptr);
void* kmalloc_zero(size_t size);

uint64_t heap_used();
uint64_t heap_total();

#endif
