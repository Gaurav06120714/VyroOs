[org 0x7c00]
[bits 16]

start:
    cli                         ; Disable interrupts
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00

    ; Print boot message via BIOS (16-bit only)
    mov si, msg_boot
    call print16

    ; Enable A20 line
    in al, 0x92
    or al, 2
    out 0x92, al

    ; Load GDT
    lgdt [gdt_descriptor]

    ; Enter 32-bit protected mode
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp CODE_SEG_32:start32

; -------------------------
; 16-bit print (BIOS)
; -------------------------
print16:
    mov ah, 0x0e
.loop:
    lodsb
    cmp al, 0
    je .done
    int 0x10
    jmp .loop
.done:
    ret

msg_boot db 'Booting VYRO OS...', 0x0d, 0x0a, 0

; -------------------------
; GDT
; -------------------------
gdt_start:
    dq 0x0

gdt_code_32:
    dw 0xffff
    dw 0x0000
    db 0x00
    db 10011010b
    db 11001111b
    db 0x00

gdt_data_32:
    dw 0xffff
    dw 0x0000
    db 0x00
    db 10010010b
    db 11001111b
    db 0x00

gdt_code_64:
    dw 0x0000
    dw 0x0000
    db 0x00
    db 10011010b
    db 10100000b
    db 0x00

gdt_data_64:
    dw 0x0000
    dw 0x0000
    db 0x00
    db 10010010b
    db 00000000b
    db 0x00

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

CODE_SEG_32 equ gdt_code_32 - gdt_start
DATA_SEG_32 equ gdt_data_32 - gdt_start
CODE_SEG_64 equ gdt_code_64 - gdt_start
DATA_SEG_64 equ gdt_data_64 - gdt_start

; -------------------------
; 32-bit Protected Mode
; -------------------------
[bits 32]
start32:
    mov ax, DATA_SEG_32
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000

    call setup_paging

    ; Enable PAE
    mov eax, cr4
    or eax, (1 << 5)
    mov cr4, eax

    ; Load PML4 into CR3
    mov eax, 0x1000
    mov cr3, eax

    ; Enable long mode via EFER
    mov ecx, 0xC0000080
    rdmsr
    or eax, (1 << 8)
    wrmsr

    ; Enable paging — activates long mode
    mov eax, cr0
    or eax, (1 << 31)
    mov cr0, eax

    jmp CODE_SEG_64:start64

; -------------------------
; Page table setup — identity map first 2MB
; -------------------------
setup_paging:
    ; Zero out 4 pages (16KB) at 0x1000
    mov edi, 0x1000
    mov ecx, 0x1000
    xor eax, eax
    rep stosd

    ; PML4[0] → PDPT at 0x2000
    mov dword [0x1000], 0x2000 | 3

    ; PDPT[0] → PD at 0x3000
    mov dword [0x2000], 0x3000 | 3

    ; PD[0] → 2MB page at 0x0 (huge page)
    mov dword [0x3000], 0x0000 | (1 << 7) | 3

    ret

; -------------------------
; 64-bit Long Mode
; -------------------------
[bits 64]
start64:
    mov ax, DATA_SEG_64
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov rsp, 0x90000

    ; Clear the screen
    mov rdi, 0xb8000
    mov rcx, 80 * 25
    mov ax, 0x0f20             ; Space, white on black
    rep stosw

    ; Print message to video memory
    mov rbx, 0xb8000
    mov rsi, msg_64
    mov ah, 0x0f               ; White on black

.print:
    lodsb
    cmp al, 0
    je .done
    mov [rbx], ax
    add rbx, 2
    jmp .print

.done:
    ; Halt safely
    cli
.halt:
    hlt
    jmp .halt

msg_64 db 'VYRO OS - 64-bit Long Mode Active', 0

times 510-($-$$) db 0
dw 0xaa55
