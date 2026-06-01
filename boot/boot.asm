[org 0x7c00]
[bits 16]

; Memory map:
;   0x1000 = PML4  |  0x2000 = PDPT  |  0x3000 = PD
;   0x7C00 = Bootloader
;   0x10000 = Kernel

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00
    mov [boot_drive], dl

    ; Load kernel from sectors 2..33 → 0x10000
    call load_kernel

    ; Enable A20
    in al, 0x92
    or al, 2
    out 0x92, al

    lgdt [gdt_descriptor]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp dword CODE32:pm32

; ──────────────────────────────────────────────
; CHS disk read: 32 sectors from sector 2 → 0x10000
; ──────────────────────────────────────────────
load_kernel:
    mov ax, 0x1000
    mov es, ax
    xor bx, bx
    mov ah, 0x02
    mov al, 32
    mov ch, 0
    mov cl, 2
    mov dh, 0
    mov dl, [boot_drive]
    int 0x13
    jc disk_error
    xor ax, ax
    mov es, ax
    ret

disk_error:
    jmp $

; ──────────────────────────────────────────────
; GDT
; ──────────────────────────────────────────────
gdt_start:
    dq 0x0
gdt_code32: dw 0xFFFF, 0x0000
            db 0x00, 10011010b, 11001111b, 0x00
gdt_data32: dw 0xFFFF, 0x0000
            db 0x00, 10010010b, 11001111b, 0x00
gdt_code64: dw 0x0000, 0x0000
            db 0x00, 10011010b, 10100000b, 0x00
gdt_data64: dw 0x0000, 0x0000
            db 0x00, 10010010b, 00000000b, 0x00
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

CODE32 equ gdt_code32 - gdt_start
DATA32 equ gdt_data32 - gdt_start
CODE64 equ gdt_code64 - gdt_start
DATA64 equ gdt_data64 - gdt_start

boot_drive db 0

; ──────────────────────────────────────────────
; 32-bit Protected Mode
; ──────────────────────────────────────────────
[bits 32]
pm32:
    mov ax, DATA32
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000

    ; Zero page tables at 0x1000–0x3FFF
    mov edi, 0x1000
    mov ecx, 0xC00
    xor eax, eax
    rep stosd

    ; PML4 → PDPT → PD
    mov dword [0x1000], 0x2003
    mov dword [0x2000], 0x3003

    ; Identity map 0–8MB (covers bootloader, kernel, VGA)
    mov dword [0x3000 + 0],  0x000083
    mov dword [0x3000 + 8],  0x200083
    mov dword [0x3000 + 16], 0x400083
    mov dword [0x3000 + 24], 0x600083

    ; Enable PAE
    mov eax, cr4
    or eax, (1 << 5)
    mov cr4, eax

    ; CR3 = PML4
    mov eax, 0x1000
    mov cr3, eax

    ; Enable long mode
    mov ecx, 0xC0000080
    rdmsr
    or eax, (1 << 8)
    wrmsr

    ; Enable paging
    mov eax, cr0
    or eax, (1 << 31)
    mov cr0, eax

    jmp CODE64:lm64

; ──────────────────────────────────────────────
; 64-bit Long Mode → jump to kernel
; ──────────────────────────────────────────────
[bits 64]
lm64:
    mov ax, DATA64
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov rsp, 0x90000

    mov rax, 0x10000
    jmp rax

times 510-($-$$) db 0
dw 0xAA55
