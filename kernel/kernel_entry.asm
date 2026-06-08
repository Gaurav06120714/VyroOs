[bits 64]
[extern kernel_main]
[global _start]

_start:
    mov rsp, 0x90000
    xor rbp, rbp
    call kernel_main

.halt:
    cli
    hlt
    jmp .halt
