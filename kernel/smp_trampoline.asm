

[BITS 16]
[ORG 0x8000]

ap_start:
    cli
    cld
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax

    lgdt [gdt_desc]

    mov eax, cr0
    or  eax, 1
    mov cr0, eax

    jmp 0x08:pm32_start

[BITS 32]
pm32_start:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x7C00

    mov eax, cr4
    or  eax, 1 << 5
    mov cr4, eax

    mov eax, [0x9008]
    mov cr3, eax

    mov ecx, 0xC0000080
    rdmsr
    or  eax, 1 << 8
    wrmsr

    mov eax, cr0
    or  eax, 1 << 31
    mov cr0, eax

    jmp 0x18:lm64_start

[BITS 64]
lm64_start:
    mov ax, 0x20
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    lock inc byte [0x9000]

    mov eax, [0xFEE00020]
    shr eax, 24

    mov ecx, eax
    inc eax
    shl rax, 16
    add rax, 0x200000
    sub rax, 8
    mov rsp, rax

    mov edi, ecx
    mov rax, [0x9010]
    call rax

.hltlp:
    hlt
    jmp .hltlp

align 16
gdt:
    dq 0
    dq 0x00CF9A000000FFFF
    dq 0x00CF92000000FFFF
    dq 0x00AF9A000000FFFF
    dq 0x00CF92000000FFFF
gdt_end:
gdt_desc:
    dw gdt_end - gdt - 1
    dd gdt
