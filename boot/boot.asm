[org 0x7c00]
[bits 16]

KERNEL_LOAD_SEGMENT equ 0x1000    ; Load kernel to 0x1000:0x0000 = 0x10000
KERNEL_SECTORS      equ 64        ; Load 64 sectors = 32KB (enough for kernel)
KERNEL_START_LBA    equ 1         ; Kernel starts at LBA sector 1 (right after bootloader)

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00
    sti

    ; Save boot drive number (BIOS puts it in DL)
    mov [boot_drive], dl

    mov si, msg_boot
    call print16

    ; Load kernel from disk
    call load_kernel

    mov si, msg_loaded
    call print16

    ; Enable A20 line
    in al, 0x92
    or al, 2
    out 0x92, al

    cli

    ; Load GDT
    lgdt [gdt_descriptor]

    ; Enter 32-bit protected mode
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp CODE_SEG_32:start32

; ─────────────────────────────────────────────────
; load_kernel: use BIOS int 0x13 extended read
; Loads KERNEL_SECTORS sectors from disk into
; physical address 0x10000
; ─────────────────────────────────────────────────
load_kernel:
    mov ah, 0x42            ; Extended read function
    mov dl, [boot_drive]    ; Drive number
    mov si, dap             ; DS:SI = Disk Address Packet
    int 0x13
    jc disk_error           ; CF set = error
    ret

disk_error:
    mov si, msg_disk_error
    call print16
.halt:
    hlt
    jmp .halt

; ─────────────────────────────────────────────────
; print16: BIOS teletype print (16-bit only)
; ─────────────────────────────────────────────────
print16:
    push ax
    mov ah, 0x0e
.loop:
    lodsb
    cmp al, 0
    je .done
    int 0x10
    jmp .loop
.done:
    pop ax
    ret

; ─────────────────────────────────────────────────
; Data
; ─────────────────────────────────────────────────
boot_drive      db 0
msg_boot        db 'VYRO OS Bootloader v0.3', 0x0d, 0x0a, 0
msg_loaded      db 'Kernel loaded. Entering 64-bit mode...', 0x0d, 0x0a, 0
msg_disk_error  db 'DISK ERROR - Cannot load kernel!', 0x0d, 0x0a, 0

; Disk Address Packet for BIOS extended read
align 4
dap:
    db 0x10                 ; DAP size (16 bytes)
    db 0x00                 ; Reserved
    dw KERNEL_SECTORS       ; Sectors to read
    dw 0x0000               ; Destination offset
    dw KERNEL_LOAD_SEGMENT  ; Destination segment (0x1000 << 4 = 0x10000)
    dq KERNEL_START_LBA     ; LBA start sector

; ─────────────────────────────────────────────────
; GDT — Global Descriptor Table
; ─────────────────────────────────────────────────
gdt_start:
    dq 0x0000000000000000   ; Null descriptor (required)

gdt_code_32:                ; 32-bit code: base=0, limit=4GB, ring 0
    dw 0xFFFF               ; Limit low
    dw 0x0000               ; Base low
    db 0x00                 ; Base mid
    db 10011010b            ; Access: present, ring 0, code, readable
    db 11001111b            ; Flags: 4KB gran, 32-bit, limit high=0xF
    db 0x00                 ; Base high

gdt_data_32:                ; 32-bit data: base=0, limit=4GB, ring 0
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10010010b            ; Access: present, ring 0, data, writable
    db 11001111b
    db 0x00

gdt_code_64:                ; 64-bit code segment (long mode)
    dw 0x0000
    dw 0x0000
    db 0x00
    db 10011010b            ; Access: present, ring 0, code, readable
    db 10100000b            ; Flags: long mode bit set (bit 5)
    db 0x00

gdt_data_64:                ; 64-bit data segment
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

; ─────────────────────────────────────────────────
; 32-bit Protected Mode
; ─────────────────────────────────────────────────
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

    ; Enable PAE (required for long mode)
    mov eax, cr4
    or eax, (1 << 5)
    mov cr4, eax

    ; Point CR3 to PML4 table at 0x1000
    mov eax, 0x1000
    mov cr3, eax

    ; Enable long mode in EFER MSR
    mov ecx, 0xC0000080
    rdmsr
    or eax, (1 << 8)        ; Set LME (Long Mode Enable)
    wrmsr

    ; Enable paging (this activates long mode)
    mov eax, cr0
    or eax, (1 << 31)
    mov cr0, eax

    jmp CODE_SEG_64:start64

; ─────────────────────────────────────────────────
; setup_paging: identity map first 2MB using 2MB huge pages
;
; Memory layout:
;   0x1000 = PML4 table
;   0x2000 = PDPT (Page Directory Pointer Table)
;   0x3000 = PD   (Page Directory) with 2MB huge page
; ─────────────────────────────────────────────────
setup_paging:
    ; Zero out 3 pages (12KB) starting at 0x1000
    mov edi, 0x1000
    mov ecx, 0xC00          ; 3 * 1024 dwords
    xor eax, eax
    rep stosd

    ; PML4[0] → PDPT at 0x2000 (Present + Writable)
    mov dword [0x1000], 0x2000 | 0x3

    ; PDPT[0] → PD at 0x3000 (Present + Writable)
    mov dword [0x2000], 0x3000 | 0x3

    ; PD[0] → 2MB huge page at 0x0 (Present + Writable + Page Size)
    mov dword [0x3000], 0x0000 | 0x83

    ; Also map 0x100000 kernel area
    ; PD[0] already covers 0x0 to 0x200000 — kernel at 0x100000 is covered
    ret

; ─────────────────────────────────────────────────
; 64-bit Long Mode
; Jump to kernel loaded at 0x10000
; ─────────────────────────────────────────────────
[bits 64]
start64:
    mov ax, DATA_SEG_64
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov rsp, 0x90000

    ; Jump to kernel entry point at 0x10000
    ; (loaded by BIOS from disk sectors)
    jmp 0x10000

; ─────────────────────────────────────────────────
; Boot signature — MUST be last 2 bytes of sector 0
; ─────────────────────────────────────────────────
times 510-($-$$) db 0
dw 0xaa55
