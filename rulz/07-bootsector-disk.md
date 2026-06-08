# LESSON 07 — Boot Sector Disk Read
> Source: https://github.com/cfenollosa/os-tutorial/tree/master/07-bootsector-disk

## Concepts
disk CHS addressing, BIOS int 0x13, carry bit, sectors

## Goal
Read kernel code from disk into memory using BIOS disk services.

## Theory
The boot sector is only 512 bytes — your kernel won't fit. The rest of your OS
lives on disk sectors after sector 1. Use BIOS `int 0x13` to load them.

### CHS Addressing (Cylinder-Head-Sector)
- **Cylinder**: concentric track number (starts at 0)
- **Head**: which side of the platter (starts at 0)
- **Sector**: segment of a track (starts at 1, not 0!)
- Boot sector = Cylinder 0, Head 0, Sector 1
- Your kernel = Cylinder 0, Head 0, Sector 2+

### Carry Bit
```nasm
mov ax, 0xFFFF
add ax, 1       ; ax = 0x0000, carry flag = 1 (overflow)
jc  error       ; jc = jump if carry is set
```
BIOS sets carry flag if disk read fails.

## The Code

**File: `boot/boot_sect_disk.asm`**
```nasm
disk_load:
    ; IN: dh = number of sectors to read, dl = drive number
    ; Reads into ES:BX
    pusha
    push dx         ; save dx (we'll check dh later)

    mov ah, 0x02    ; BIOS read sector function
    mov al, dh      ; number of sectors to read
    mov cl, 0x02    ; start from sector 2 (sector 1 = boot sector)
    mov ch, 0x00    ; cylinder 0
    mov dh, 0x00    ; head 0

    int 0x13        ; BIOS disk interrupt
    jc  disk_error  ; carry set = error

    ; Verify sectors read
    pop dx
    cmp al, dh
    jne sectors_error

    popa
    ret

disk_error:
    mov bx, DISK_ERROR_MSG
    call print_string
    jmp $

sectors_error:
    mov bx, SECTORS_ERROR_MSG
    call print_string
    jmp $

DISK_ERROR_MSG:
    db 'Disk read error!', 0
SECTORS_ERROR_MSG:
    db 'Incorrect number of sectors read!', 0
```

**Usage in main boot sector:**
```nasm
[org 0x7C00]

mov [BOOT_DRIVE], dl    ; BIOS puts drive number in dl at startup — save it!

; Set up destination in memory
mov bx, 0x0000
mov es, bx
mov bx, 0x8000          ; load kernel at 0x8000

mov dh, 2               ; read 2 sectors
mov dl, [BOOT_DRIVE]
call disk_load

; Jump to loaded code
jmp 0x8000

BOOT_DRIVE:
    db 0
```

## Build Steps
```bash
nasm -f bin boot_sect_main.asm -o boot_sect_main.bin
qemu-system-i386 -fda boot_sect_main.bin
# OR
qemu-system-i386 boot_sect_main.bin -boot c
```

## Common Problems & Solutions

| Problem | Solution |
|---------|----------|
| Disk read error on QEMU | Use `-fda` flag: `qemu-system-i386 -fda boot_sect_main.bin` (sets `dl=0x00`) |
| Wrong drive number | BIOS puts drive in `dl` on boot — save it immediately: `mov [BOOT_DRIVE], dl` |
| Loaded code doesn't run correctly | Verify load address — if kernel expects `0x1000`, load to `0x1000`, jump to `0x1000` |
| `al` ≠ requested sectors | BIOS returns actual sectors read in `al` — compare with requested count |
| Sector number starts at 0? | NO — sectors are 1-indexed! Sector 1 = boot sector, sector 2 = first after it |

## Rules to Remember
- Sectors are **1-indexed** — sector 1 is boot sector, your kernel starts at sector 2
- ALWAYS save `dl` on entry — BIOS passes drive number there
- Check carry flag after `int 0x13` — carry = error
- Compare `al` (sectors actually read) with `dh` (sectors requested)
- Set `ES:BX` = destination address before calling disk_load
