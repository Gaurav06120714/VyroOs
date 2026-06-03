#include "../include/types.h"

// Assembled binary of smp_trampoline.asm, brought in via the linker.
__asm__(
    ".section .rodata\n"
    ".global smp_trampoline_blob\n"
    ".global smp_trampoline_blob_end\n"
    "smp_trampoline_blob:\n"
    ".incbin \"build/smp_trampoline.bin\"\n"
    "smp_trampoline_blob_end:\n"
    ".section .text\n"
);

extern uint8_t smp_trampoline_blob[];
extern uint8_t smp_trampoline_blob_end[];

uint32_t smp_trampoline_blob_len(void) {
    return (uint32_t)(smp_trampoline_blob_end - smp_trampoline_blob);
}
