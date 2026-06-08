#include "heap.h"

typedef struct block_header {
    size_t               size;
    uint8_t              free;
    struct block_header* next;
    uint32_t             magic;
} block_header_t;

#define HEAP_MAGIC   0xDEADBEEF
#define HEADER_SIZE  sizeof(block_header_t)

static block_header_t* heap_head = 0;
static uint64_t        bytes_used = 0;

void heap_init() {
    heap_head        = (block_header_t*) HEAP_START;
    heap_head->size  = HEAP_SIZE - HEADER_SIZE;
    heap_head->free  = 1;
    heap_head->next  = 0;
    heap_head->magic = HEAP_MAGIC;
    bytes_used       = 0;
}

static void coalesce() {
    block_header_t* cur = heap_head;
    while (cur && cur->next) {
        if (cur->free && cur->next->free) {
            cur->size += HEADER_SIZE + cur->next->size;
            cur->next  = cur->next->next;
        } else {
            cur = cur->next;
        }
    }
}

void* kmalloc(size_t size) {
    if (size == 0) return 0;


    size = (size + 7) & ~7UL;

    block_header_t* cur = heap_head;
    while (cur) {
        if (cur->free && cur->size >= size) {

            if (cur->size >= size + HEADER_SIZE + 16) {
                block_header_t* new_block = (block_header_t*)
                    ((uint8_t*)cur + HEADER_SIZE + size);
                new_block->size  = cur->size - size - HEADER_SIZE;
                new_block->free  = 1;
                new_block->next  = cur->next;
                new_block->magic = HEAP_MAGIC;
                cur->next = new_block;
                cur->size = size;
            }
            cur->free = 0;
            bytes_used += cur->size;
            return (void*)((uint8_t*)cur + HEADER_SIZE);
        }
        cur = cur->next;
    }
    return 0;
}

void kfree(void* ptr) {
    if (!ptr) return;
    block_header_t* block = (block_header_t*)((uint8_t*)ptr - HEADER_SIZE);
    if (block->magic != HEAP_MAGIC) return;
    if (block->free) return;
    block->free = 1;
    if (bytes_used >= block->size) bytes_used -= block->size;
    coalesce();
}

void* kmalloc_zero(size_t size) {
    void* ptr = kmalloc(size);
    if (!ptr) return 0;
    uint8_t* p = (uint8_t*)ptr;
    for (size_t i = 0; i < size; i++) p[i] = 0;
    return ptr;
}

uint64_t heap_used()  { return bytes_used; }
uint64_t heap_total() { return HEAP_SIZE; }
