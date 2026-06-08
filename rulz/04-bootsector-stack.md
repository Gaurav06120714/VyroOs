# LESSON 04 — Boot Sector Stack
> Source: https://github.com/cfenollosa/os-tutorial/tree/master/04-bootsector-stack

## Concepts
stack, bp register, sp register, push, pop

## Goal
Set up and use the x86 stack in the boot sector.

## Theory
The stack is a region of memory for temporary storage. It grows **downward**:
- `bp` (base pointer) = bottom of stack (stays fixed)
- `sp` (stack pointer) = top of stack (moves down when you push)
- `push` → decrements `sp`, writes value
- `pop` → reads value, increments `sp`
- `call` → pushes return address, jumps to function
- `ret` → pops return address, jumps back

## The Code

**File: `boot/boot_sect_stack.asm`**
```nasm
[org 0x7C00]

mov ah, 0x0e    ; tty print mode

; Set up stack above the boot sector (safe zone)
mov bp, 0x8000  ; base of stack — above 0x7C00
mov sp, bp      ; stack pointer starts at base

; Push some values
push 'A'
push 'B'
push 'C'

; Pop them (LIFO — last in, first out)
pop bx
mov al, bl
int 0x10    ; prints 'C'

pop bx
mov al, bl
int 0x10    ; prints 'B'

pop bx
mov al, bl
int 0x10    ; prints 'A'

jmp $

times 510-($-$$) db 0
dw 0xaa55
```

## Build Steps
```bash
nasm -f bin boot_sect_stack.asm -o stack.bin
qemu-system-i386 -fda stack.bin
```

## Expected Result
Screen prints `CBA` — reversed order because stack is LIFO.

## Common Problems & Solutions

| Problem | Solution |
|---------|----------|
| Stack corrupts boot sector code | Stack pointer set too low — set `bp` to `0x8000` or higher, safely above `0x7DFF` |
| `pop` gives wrong value | Stack is LIFO — last pushed = first popped. If you pushed A,B,C, you pop C,B,A |
| `call` crashes | You forgot `ret` at end of subroutine — `call` pushes return address, `ret` must pop it |
| Stack underflow | Popping more times than you pushed — reads garbage memory |

## Rules to Remember
- Set stack base ABOVE `0x7DFF` (end of boot sector) — `0x8000` is safe
- Stack grows DOWN: `push` decrements `sp`, `pop` increments it  
- `push word` pushes 2 bytes; `push dword` pushes 4 bytes
- Always `ret` from every `call` — they must balance
