# LESSON 05 — Boot Sector Functions & Strings
> Source: https://github.com/cfenollosa/os-tutorial/tree/master/05-bootsector-functions-strings

## Concepts
control structures, function calling, strings, %include, null terminator

## Goal
Write reusable print functions, null-terminated strings, and use `%include` for file splitting.

## Strings in Assembly
```nasm
my_string:
    db 'Hello, World', 0   ; null-terminated (like C strings)
```
The trailing `0` = null byte (`0x00`) — marks the end of string.

## Control Structures
```nasm
cmp ax, 4       ; compare ax with 4
je  equal       ; jump if equal
jne not_equal   ; jump if not equal
jl  less        ; jump if less
jg  greater     ; jump if greater
jmp forever     ; unconditional jump
```

## Functions with call/ret
```nasm
; Calling a function
mov al, 'X'     ; parameter: character to print
call print_char
; execution resumes here after ret

print_char:
    pusha           ; save ALL registers to stack
    mov ah, 0x0e
    int 0x10        ; print char in al
    popa            ; restore ALL registers from stack
    ret             ; return to caller
```

## Print String Function
```nasm
; Print null-terminated string. Address of string in bx.
print_string:
    pusha
.loop:
    mov al, [bx]        ; load next character
    cmp al, 0           ; check for null terminator
    je  .done           ; if null, we're done
    mov ah, 0x0e
    int 0x10            ; print char
    inc bx              ; next character
    jmp .loop
.done:
    popa
    ret
```

## Print Hex Function
```nasm
; Print value of dx as 4 hex digits
print_hex:
    pusha
    mov cx, 0           ; counter
.loop:
    cmp cx, 4
    je .done
    mov ax, dx
    and ax, 0x000F      ; isolate last nibble
    add al, 0x30        ; convert to ASCII digit
    cmp al, 0x39        ; if > '9'
    jle .print
    add al, 7           ; shift to 'A'-'F'
.print:
    ; store char at correct position in hex_out
    mov bx, HEX_OUT + 5
    sub bx, cx
    mov [bx], al
    ror dx, 4           ; rotate right 4 bits (next nibble)
    inc cx
    jmp .loop
.done:
    mov bx, HEX_OUT
    call print_string
    popa
    ret

HEX_OUT:
    db '0x0000', 0
```

## Including External Files
```nasm
%include "boot_sect_print.asm"      ; include print functions
%include "boot_sect_print_hex.asm"  ; include hex print
```

## Build Steps
```bash
nasm -f bin boot_sect_main.asm -o boot_sect_main.bin
qemu-system-i386 -fda boot_sect_main.bin
```

## Common Problems & Solutions

| Problem | Solution |
|---------|----------|
| Function never returns | Missing `ret` at end of function |
| Registers trashed after call | Use `pusha`/`popa` at start/end of every function |
| String prints garbage | Missing null terminator `0` at end of `db` string |
| `%include` file not found | File must be in same directory as the main `.asm` file |
| Newline doesn't work | Newline = TWO bytes: `0x0A` (newline) + `0x0D` (carriage return) |

## Rules to Remember
- Null-terminate every string with `, 0`
- Use `pusha`/`popa` in functions to avoid clobbering caller's registers
- Use `call`/`ret` — never bare `jmp` for functions (can't return)
- Pass parameters in agreed registers (e.g., string address in `bx`)
