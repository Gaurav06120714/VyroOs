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
; LBA extended read (int 0x13 AH=42h) in chunks.
; Reads 192 sectors from LBA 1 → 0x10000, 16 at a time
; (16-sector chunks never cross track/BIOS limits).
; ──────────────────────────────────────────────
load_kernel:
    mov word [dap_count], 16        ; sectors per chunk
    mov word [dap_seg],   0x1000    ; dest segment (0x1000:0 = 0x10000)
    mov word [dap_off],   0x0000
    mov dword [dap_lba],  1         ; start LBA (sector 1, after boot sector)
    mov cx, 48                      ; 48 chunks * 16 = 768 sectors = 384KB

.read_chunk:
    push cx
    mov ah, 0x42
    mov dl, [boot_drive]
    mov si, dap
    int 0x13
    jc disk_error
    pop cx

    add word [dap_seg], 0x200       ; advance 16 sectors = 0x2000 bytes = 0x200 paragraphs
    add dword [dap_lba], 16
    dec cx
    jnz .read_chunk

    xor ax, ax
    mov es, ax
    ret

disk_error:
    mov ah, 0x0e
    mov al, 'D'
    int 0x10
    jmp $

; Disk Address Packet for extended read
align 4
dap:
    db 0x10                 ; DAP size
    db 0x00                 ; reserved
dap_count: dw 16            ; sectors to read
dap_off:   dw 0x0000        ; dest offset
dap_seg:   dw 0x1000        ; dest segment
dap_lba:   dq 1             ; start LBA

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

    ; ──────────────────────────────────────────
    ; Full 0-4GB identity map using 2MB pages.
    ; This guarantees the VBE framebuffer (wherever QEMU places it,
    ; e.g. 0xFD000000) is always mapped — no fragile per-LFB math.
    ;
    ; vC.6.10.9: page directories moved from 0x70000..0x73FFF up to
    ; 0x200000..0x203FFF (the "physical RAM under PMM" region 2-5 MB)
    ; because the kernel BSS grew past 0x70000 as new subsystems
    ; were added (memmap, block, parttab, lba_xlate from vC.6.5-10),
    ; and BSS zeroing was wiping out page-table entries causing
    ; eventual page faults on heap allocations whose translations
    ; happened to live in the clobbered PDE pages.
    ;
    ; Layout:
    ;   0x1000   = PML4
    ;   0x2000   = PDPT (4 entries → 4 PDs covering 4GB)
    ;   0x200000 = 4 Page Directories (2048 × 2MB entries, 16 KB total)
    ; ──────────────────────────────────────────

    ; Zero PML4 + PDPT (0x1000–0x2FFF)
    mov edi, 0x1000
    mov ecx, 0x800          ; 2048 dwords = 8KB
    xor eax, eax
    rep stosd

    ; Zero the 4 page directories (0x200000–0x203FFF)
    mov edi, 0x200000
    mov ecx, 0x1000         ; 4096 dwords = 16KB
    xor eax, eax
    rep stosd

    ; PML4[0] → PDPT  (User bit set so ring 3 can walk the tables)
    mov dword [0x1000], 0x2007

    ; PDPT[0..3] → the 4 page directories at 0x200000+
    mov dword [0x2000 + 0],  0x200007
    mov dword [0x2000 + 8],  0x201007
    mov dword [0x2000 + 16], 0x202007
    mov dword [0x2000 + 24], 0x203007

    ; Fill 2048 contiguous 2MB page entries (0x200000 as one flat array)
    ; entry[j] = (j * 2MB) | 0x87  (present | writable | user | huge)
    mov edi, 0x200000
    xor ecx, ecx            ; j = 0
.map_4gb:
    mov eax, ecx
    shl eax, 21             ; physical addr = j * 2MB
    or  eax, 0x87           ; present + writable + user + huge page
    mov [edi], eax
    mov dword [edi + 4], 0  ; high 32 bits = 0
    add edi, 8
    inc ecx
    cmp ecx, 2048
    jl .map_4gb

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
