; SMP AP trampoline: 16-bit real → 32-bit protected → 64-bit long mode → C.
; Copied to physical 0x8000 at runtime.
;   BSP publishes:
;     [0x9008] = PML4 phys
;     [0x9010] = ap_main address (8 bytes)
;     [0x9000] = AP-online byte counter (incremented atomically by AP)

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

    ; Read LAPIC ID (MMIO at 0xFEE00020, ID in upper 8 bits)
    mov eax, [0xFEE00020]
    shr eax, 24                          ; eax = apic id

    ; Per-CPU stack: top = 0x100000 + (apic_id+1) * 0x10000 - 8
    mov ecx, eax                         ; save id
    inc eax
    shl rax, 16
    add rax, 0x100000
    sub rax, 8
    mov rsp, rax

    ; Call ap_main(apic_id) — first arg in EDI per System V ABI
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
