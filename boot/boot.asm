[org 0x7c00]
[bits 16]

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00
    sti

    mov [boot_drive], dl

    mov ax, 0x0003
    int 0x10

    mov ax, 0x4F01
    mov cx, 0x0118
    mov di, 0x0600
    int 0x10

    cmp ax, 0x004F
    jne .vbe_fail

    mov eax, [0x0628]
    mov [0x0500], eax

    push es
    mov ax, 0x1130
    mov bh, 0x06
    int 0x10

    push ds
    mov ax, es
    mov ds, ax
    mov si, bp
    mov ax, 0x8000
    mov es, ax
    xor di, di
    mov cx, 0x1000
    rep movsb
    pop ds
    pop es

    mov ax, 0x4F02
    mov bx, 0x4118
    int 0x10

    cmp ax, 0x004F
    jne .vbe_fail
    jmp .vbe_done

.vbe_fail:

    xor eax, eax
    mov [0x0500], eax

.vbe_done:

    cli

    call load_kernel

    in al, 0x92
    or al, 2
    out 0x92, al

    lgdt [gdt_descriptor]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp dword CODE32:pm32

load_kernel:
    mov word [dap_count], 16
    mov word [dap_seg],   0x1000
    mov word [dap_off],   0x0000
    mov dword [dap_lba],  1
    mov cx, 48

.read_chunk:
    push cx
    mov ah, 0x42
    mov dl, [boot_drive]
    mov si, dap
    int 0x13
    jc disk_error
    pop cx

    add word [dap_seg], 0x200
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

align 4
dap:
    db 0x10
    db 0x00
dap_count: dw 16
dap_off:   dw 0x0000
dap_seg:   dw 0x1000
dap_lba:   dq 1

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

[bits 32]
pm32:
    mov ax, DATA32
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000

    mov edi, 0x1000
    mov ecx, 0x800
    xor eax, eax
    rep stosd

    mov edi, 0x200000
    mov ecx, 0x1000
    xor eax, eax
    rep stosd

    mov dword [0x1000], 0x2007

    mov dword [0x2000 + 0],  0x200007
    mov dword [0x2000 + 8],  0x201007
    mov dword [0x2000 + 16], 0x202007
    mov dword [0x2000 + 24], 0x203007

    mov edi, 0x200000
    xor ecx, ecx
.map_4gb:
    mov eax, ecx
    shl eax, 21
    or  eax, 0x87
    mov [edi], eax
    mov dword [edi + 4], 0
    add edi, 8
    inc ecx
    cmp ecx, 2048
    jl .map_4gb

    mov eax, cr4
    or eax, (1 << 5)
    mov cr4, eax

    mov eax, 0x1000
    mov cr3, eax

    mov ecx, 0xC0000080
    rdmsr
    or eax, (1 << 8)
    wrmsr

    mov eax, cr0
    or eax, (1 << 31)
    mov cr0, eax

    jmp CODE64:lm64

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
