[org 0x7c00]
[bits 16]

; Memory map:
;   0x0500 = LFB base address (dword, stored by VBE init)
;   0x0600 = VBE mode info block (256 bytes)
;   0x1000 = PML4  |  0x2000 = PDPT  |  0x3000 = PD  |  0x4000 = PD for LFB
;   0x7C00 = Bootloader
;   0x10000 = Kernel
;   0x80000 = BIOS 8x16 font (256*16 = 4096 bytes)

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00
    sti

    mov [boot_drive], dl

    ; Switch to text mode first (needed before VBE call)
    mov ax, 0x0003      ; Set standard 80x25 mode (clears screen)
    int 0x10

    ; ──────────────────────────────────────────────
    ; VBE: Query mode info for mode 0x118 (1024x768x24bpp)
    ; Result buffer at 0x0600 (ES:DI = 0x0000:0x0600)
    ; ──────────────────────────────────────────────
    mov ax, 0x4F01          ; VBE Get Mode Info
    mov cx, 0x0118          ; Mode 0x118 = 1024x768x24bpp
    mov di, 0x0600          ; Store result at 0x0600 (DS=0, so linear)
    int 0x10

    cmp ax, 0x004F          ; AX=0x004F means success
    jne .vbe_fail

    ; Extract LFB base address from offset 0x28 (40) of the mode info block
    mov eax, [0x0628]       ; dword at 0x0600 + 0x28
    mov [0x0500], eax       ; Save LFB address to 0x0500

    ; Copy BIOS 8x16 font to 0x80000 before we lose real-mode access
    ; int 0x10, AX=0x1130, BH=0x06 → ES:BP = pointer to 8x16 font
    push es
    mov ax, 0x1130
    mov bh, 0x06
    int 0x10
    ; ES:BP now points to font data
    ; Copy 256*16=4096 bytes from ES:BP to 0x8000:0x0000
    push ds
    mov ax, es              ; Move ES → AX, then AX → DS
    mov ds, ax              ; DS:SI = font source (ES:BP)
    mov si, bp
    mov ax, 0x8000
    mov es, ax              ; ES:DI = 0x8000:0x0000 (physical 0x80000)
    xor di, di
    mov cx, 0x1000          ; 4096 bytes (fits in 16-bit without overflow)
    rep movsb
    pop ds
    pop es

    ; Set VBE mode 0x118 with linear framebuffer (bit 14 set → 0x4118)
    mov ax, 0x4F02
    mov bx, 0x4118          ; 0x4000 = use linear framebuffer
    int 0x10

    cmp ax, 0x004F
    jne .vbe_fail
    jmp .vbe_done

.vbe_fail:
    ; Store 0 at 0x0500 to signal no VBE
    xor eax, eax
    mov [0x0500], eax

.vbe_done:

    cli

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

    ; Zero page tables at 0x1000–0x4FFF (covers PML4, PDPT, PD, PD_LFB)
    mov edi, 0x1000
    mov ecx, 0x1000         ; 4096 dwords = 16KB
    xor eax, eax
    rep stosd

    ; PML4 → PDPT → PD (first 1GB range, PDPT[0])
    mov dword [0x1000], 0x2003      ; PML4[0] → PDPT at 0x2000
    mov dword [0x2000], 0x3003      ; PDPT[0] → PD at 0x3000

    ; Identity map 0–8MB (covers bootloader, kernel, VGA, font at 0x80000)
    mov dword [0x3000 + 0],  0x000083
    mov dword [0x3000 + 8],  0x200083
    mov dword [0x3000 + 16], 0x400083
    mov dword [0x3000 + 24], 0x600083

    ; ──────────────────────────────────────────
    ; Map LFB framebuffer pages
    ; LFB address is stored at 0x0500 (set in 16-bit code above)
    ; QEMU VBE LFB is typically at 0xE0000000
    ; We use PDPT[3] → new PD at 0x4000 to cover the 3rd GB (0xC0000000-0xFFFFFFFF)
    ; ──────────────────────────────────────────
    mov eax, [0x0500]       ; Load LFB address
    test eax, eax
    jz .skip_lfb_map        ; If zero, VBE failed, skip

    ; PDPT[3] → PD_LFB at 0x4000
    mov dword [0x2000 + 3*8], 0x4003   ; PDPT[3] → 0x4000, present+writable

    ; Calculate PD index: (LFB_addr - 0xC0000000) >> 21
    ; Each PD entry covers 2MB. PDPT[3] covers 0xC0000000–0xFFFFFFFF.
    mov ebx, eax            ; ebx = LFB base
    sub ebx, 0xC0000000     ; offset within the 3rd GB
    shr ebx, 21             ; PD index (2MB per entry)

    ; Map 6 consecutive 2MB huge pages starting at LFB address
    ; Flags: 0x83 = present | writable | huge (PS bit)
    mov ecx, 0             ; loop counter
.lfb_map_loop:
    cmp ecx, 6
    jge .skip_lfb_map

    ; PD entry index = ebx + ecx
    mov edx, ebx
    add edx, ecx            ; PD index for this entry
    shl edx, 3              ; * 8 bytes per entry

    ; Entry value: LFB_base + ecx*2MB | 0x83
    mov edi, ecx
    shl edi, 21             ; ecx * 2MB
    add edi, eax            ; + LFB base address
    or edi, 0x83            ; present | writable | huge

    mov [0x4000 + edx], edi
    mov dword [0x4000 + edx + 4], 0    ; high 32 bits = 0

    inc ecx
    jmp .lfb_map_loop

.skip_lfb_map:

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
