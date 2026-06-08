# LESSON 03 — Boot Sector Memory
> Source: https://github.com/cfenollosa/os-tutorial/tree/master/03-bootsector-memory

## Concepts
memory offsets, pointers, org directive, 0x7C00

## Goal
Understand how the x86 memory is laid out at boot time and why `0x7C00` matters.

## Theory
The BIOS loads your 512-byte boot sector at physical address `0x7C00`.
Any data labels in your code resolve relative to `0x0000` by default —
but they actually live at `0x7C00`. This mismatch causes bugs.

### Real Mode Memory Map
```
0x00000 – 0x003FF   Interrupt Vector Table (IVT)
0x00400 – 0x004FF   BIOS Data Area
0x07C00 – 0x07DFF   YOUR BOOT SECTOR (512 bytes)
0x07E00 +           Free memory
0x9FFFF             End of conventional memory (640 KB)
0xA0000 +           Video memory / ROM BIOS
```

## The Bug and Fix

### Wrong (no org):
```nasm
the_secret:
    db "X"

mov al, [the_secret]   ; BUG: thinks the_secret is at 0x002d, not 0x7C2d
int 0x10               ; prints garbage
```

### Correct approach 1 — add base manually:
```nasm
mov al, [the_secret + 0x7C00]   ; manually offset
```

### Correct approach 2 — use `org` (canonical):
```nasm
[org 0x7C00]           ; tell NASM: all addresses are based at 0x7C00

the_secret:
    db "X"

mov al, [the_secret]   ; NOW works — nasm adds 0x7C00 automatically
```

## Build Steps
```bash
nasm -f bin boot_sect_memory_org.asm -o mem.bin
qemu-system-i386 -fda mem.bin
```

## Common Problems & Solutions

| Problem | Solution |
|---------|----------|
| Prints garbage instead of your character | Missing `[org 0x7C00]` — NASM doesn't know base address |
| Works without org in some cases | You got lucky — only works if data is at address < 256 |
| `0x2d` offset used in examples | That's just the byte position of the data in that specific file — recalculate if you add/remove instructions |

## Rules to Remember
- ALWAYS put `[org 0x7C00]` at the top of your boot sector asm files
- Without `org`, all label addresses are wrong (off by `0x7C00`)
- `the_secret` with `org` = NASM auto-adds `0x7C00` to every label
