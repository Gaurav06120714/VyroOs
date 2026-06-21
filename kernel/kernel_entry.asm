[bits 64]
[extern kernel_main]
[extern __bss_start]
[extern __bss_end]
[global _start]

_start:
    ; BSS now lives at 0x100000-~0x17e208 (above the legacy ROM hole at
    ; 0xA0000-0xFFFFF). Put the stack above BSS end and below PMM-managed RAM
    ; (0x200000) so the BSS clear below cannot corrupt it.
    mov rsp, 0x1f0000
    xor rbp, rbp

    ; Zero BSS before calling kernel_main
    mov rdi, __bss_start
    mov rcx, __bss_end
    sub rcx, rdi
    xor eax, eax
    rep stosb

    call kernel_main

.halt:
    cli
    hlt
    jmp .halt
