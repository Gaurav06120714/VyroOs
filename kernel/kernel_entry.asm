[bits 64]

; Declare kernel_main as external C function
[extern kernel_main]

; Global entry point — bootloader jumps here
[global _start]

_start:
    ; Set up a proper 64-bit stack at 2MB mark
    mov rsp, 0x200000
    xor rbp, rbp            ; Clear base pointer (marks top of call stack)

    ; Call the C kernel — this should never return
    call kernel_main

    ; Safety halt if kernel_main somehow returns
.halt:
    cli
    hlt
    jmp .halt
