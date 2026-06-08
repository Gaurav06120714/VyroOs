# LESSON 02 — Boot Sector Print
> Source: https://github.com/cfenollosa/os-tutorial/tree/master/02-bootsector-print

## Concepts
BIOS interrupts, CPU registers, teletype output

## Goal
Print text to screen from the boot sector using BIOS interrupt `int 0x10`.

## Theory
BIOS provides video services via interrupt `0x10`. Setting `ah = 0x0e` activates
"teletype mode" — writes character in `al` to screen and advances the cursor.
This only works in 16-bit real mode. Once we switch to protected mode, BIOS is gone.

## The Code

**File: `boot/boot_sect_hello.asm`**
```nasm
mov ah, 0x0e    ; BIOS teletype mode

mov al, 'H'
int 0x10
mov al, 'e'
int 0x10
mov al, 'l'
int 0x10
int 0x10        ; 'l' is still in al from previous mov
mov al, 'o'
int 0x10

jmp $           ; infinite loop ($ = current address)

times 510-($-$$) db 0
dw 0xaa55
```

## Build Steps
```bash
nasm -fbin boot_sect_hello.asm -o boot_sect_hello.bin
qemu-system-i386 -fda boot_sect_hello.bin
```

## Expected Result
Screen shows: `Hello` then hangs.

## Inspect the Binary
```bash
xxd boot_sect_hello.bin   # examine the raw bytes
```

## Common Problems & Solutions

| Problem | Solution |
|---------|----------|
| Nothing prints | Check `ah = 0x0e` is set before EACH `int 0x10` call (in real code other processes may trash `ah`) |
| Garbled characters | Wrong value in `al` — ensure correct ASCII code or use quoted char `'H'` |
| Works but cursor doesn't move | `0x0e` mode auto-advances cursor — if it's not, you may be in wrong video mode |

## Rules to Remember
- `ah = 0x0e` must be set before calling `int 0x10` for teletype output
- Character goes in `al`
- This ONLY works in 16-bit real mode — no BIOS after protected mode switch
- `jmp $` = infinite loop (`$` = address of current instruction)
