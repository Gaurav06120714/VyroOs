; SMP AP trampoline: 16-bit real → 32-bit protected → 64-bit long mode.
; Copied to physical 0x8000 at runtime. BSP publishes its PML4 phys addr
; at [0x9008] before sending SIPI; AP loads it into CR3.

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
    or  eax, 1                  ; PE
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
    mov esp, 0x7C00              ; throwaway stack in low memory

    mov eax, cr4
    or  eax, 1 << 5              ; PAE
    mov cr4, eax

    mov eax, [0x9008]            ; PML4 phys published by BSP
    mov cr3, eax

    mov ecx, 0xC0000080          ; IA32_EFER
    rdmsr
    or  eax, 1 << 8              ; LME
    wrmsr

    mov eax, cr0
    or  eax, 1 << 31             ; PG
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

    lock inc byte [0x9000]       ; signal: an AP is live in long mode

.hltlp:
    hlt
    jmp .hltlp

align 16
gdt:
    dq 0
    dq 0x00CF9A000000FFFF        ; 32-bit code (sel 0x08)
    dq 0x00CF92000000FFFF        ; 32-bit data (sel 0x10)
    dq 0x00AF9A000000FFFF        ; 64-bit code (sel 0x18)
    dq 0x00CF92000000FFFF        ; 64-bit data (sel 0x20, reusing 32-bit data layout)
gdt_end:
gdt_desc:
    dw gdt_end - gdt - 1
    dd gdt
