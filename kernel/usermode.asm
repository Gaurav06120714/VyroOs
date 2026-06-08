[bits 64]

[global saved_kernel_rsp]
section .bss
saved_kernel_rsp: resq 1

section .text

[global enter_user_mode]
enter_user_mode:

    mov [rel saved_kernel_rsp], rsp

    mov ax, 0x23
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push 0x23
    push rsi
    pushfq
    pop rax
    or rax, 0x200
    push rax
    push 0x1B
    push rdi
    iretq

[global return_to_kernel]
return_to_kernel:

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    mov rsp, [rel saved_kernel_rsp]
    ret
