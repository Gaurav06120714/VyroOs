# LESSON 06 — Segmentation (Real Mode)
> Source: https://github.com/cfenollosa/os-tutorial/tree/master/06-bootsector-segmentation

## Concepts
segmentation, segment registers, real mode addressing, cs ds ss es

## Goal
Understand and use x86 real mode segment:offset memory addressing.

## Theory
In 16-bit real mode, the CPU uses **segment:offset** notation to address memory.

```
Physical Address = Segment Register × 16 + Offset
```

Example: `DS = 0x4D`, accessing `[0x20]` → physical address = `0x4D0 + 0x20 = 0x4F0`

### Segment Registers
| Register | Meaning | Used for |
|----------|---------|---------|
| `CS` | Code Segment | instruction fetches |
| `DS` | Data Segment | data reads/writes |
| `SS` | Stack Segment | stack operations |
| `ES` | Extra Segment | string operations, user-defined |

Segment registers are **implicitly used** — once you set `DS`, ALL memory reads use it.

## The Code

```nasm
[org 0x7C00]

; You CANNOT mov literal directly to segment registers
; Must use a general register as intermediary
mov ax, 0x7C0       ; segment value
mov ds, ax          ; set data segment

mov bx, my_var      ; offset only — DS is added automatically
mov al, [bx]        ; reads from DS:bx = 0x7C0*16 + offset = physical address

; Print it
mov ah, 0x0e
int 0x10

jmp $

my_var:
    db 'Z'

times 510-($-$$) db 0
dw 0xaa55
```

## Common Problems & Solutions

| Problem | Solution |
|---------|----------|
| `mov ds, 0x7C0` — invalid instruction | Cannot move literal into segment register; use `mov ax, val` → `mov ds, ax` |
| Code jumps to wrong address after `CS` change | Far jump needed: `jmp SEGMENT:OFFSET` to reload CS |
| `[org 0x7C00]` and segment registers conflict | If you use `org`, keep `DS=0` and let org handle offset; OR set `DS=0x7C0` and use no `org` — don't mix both |
| Data reads give wrong values | `DS` is wrong; verify `DS × 16 + offset = correct physical address` |

## Rules to Remember
- `segment × 16 + offset` = physical address (shift left 4 bits)
- Never `mov DS, literal` — always go through a general register
- Changing `DS` affects ALL subsequent memory accesses silently
- Use `[org 0x7C00]` + `DS=0` as the standard simple approach
