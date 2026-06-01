[bits 64]

; ─────────────────────────────────────────────────
; gdt_flush(uint64_t gdt_ptr_addr)  — rdi = &gdt_ptr
; Loads the new GDT, reloads data segment registers,
; and reloads CS (0x08) via a far return (lretq).
; ─────────────────────────────────────────────────
[global gdt_flush]
gdt_flush:
    lgdt [rdi]

    ; Reload data segment registers with kernel data (0x10)
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Reload CS with kernel code (0x08) via far return.
    ; Push new CS selector + target RIP, then lretq.
    push 0x08
    lea rax, [rel .reload_cs]
    push rax
    retfq
.reload_cs:
    ret

; ─────────────────────────────────────────────────
; tss_flush(uint16_t selector)  — rdi = selector
; ─────────────────────────────────────────────────
[global tss_flush]
tss_flush:
    mov ax, di
    ltr ax
    ret
