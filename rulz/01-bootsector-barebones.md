# LESSON 01 — Boot Sector Barebones
> Source: https://github.com/cfenollosa/os-tutorial/tree/master/01-bootsector-barebones

## Concepts
assembler, BIOS, boot sector, magic bytes

## Goal
Create the smallest file the BIOS will treat as a bootable disk.

## Theory
When a PC powers on, the BIOS looks for a bootable disk. It reads the very first 512 bytes
of disk into memory at address `0x7C00`. If bytes 511–512 are `0x55 0xAA`, it jumps to `0x7C00`.
Your boot sector runs in 16-bit real mode.

## The Code

**File: `boot/boot_sect_simple.asm`**
```nasm
; Infinite loop — do nothing, just stay alive
loop:
    jmp loop

; Pad binary to 510 bytes, then write magic signature
times 510-($-$$) db 0
dw 0xaa55
```

## Build Steps
```bash
# Assemble to raw binary
nasm -f bin boot_sect_simple.asm -o boot_sect_simple.bin

# Run in emulator
qemu-system-x86_64 boot_sect_simple.bin
# OR
qemu-system-i386 -fda boot_sect_simple.bin
```

## Expected Result
QEMU window opens, shows "Booting from Hard Disk..." — then hangs in infinite loop. That's correct.

## Common Problems & Solutions

| Problem | Solution |
|---------|----------|
| `error: invalid combination of opcode and operands` | Wrong nasm version — use Homebrew nasm, not Xcode nasm |
| QEMU shows "Boot failed: not a bootable disk" | Magic bytes `0xAA55` are missing or in wrong position — must be bytes 511–512 |
| `nasm -f bin` produces wrong size | `times 510-($-$$) db 0` fills to 510, then `dw 0xaa55` adds 2 = 512 total. If code > 510 bytes you'll get an error |
| SDL error on macOS | `qemu-system-x86_64 -nographic boot_sect_simple.bin` |

## Rules to Remember
- Boot sector is EXACTLY 512 bytes — no more, no less
- Last 2 bytes MUST be `0x55, 0xAA` (little-endian → `dw 0xAA55`)
- Loaded at memory address `0x7C00`
- CPU starts in 16-bit real mode
