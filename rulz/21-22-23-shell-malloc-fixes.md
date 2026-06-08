# LESSON 21-22-23 — Shell, Malloc & Bug Fixes
> Sources:
> - https://github.com/cfenollosa/os-tutorial/tree/master/21-shell
> - https://github.com/cfenollosa/os-tutorial/tree/master/22-malloc
> - https://github.com/cfenollosa/os-tutorial/tree/master/23-fixes

## Concepts
shell, readline, keyboard buffer, kmalloc, bump allocator, stdint, size_t, ABI fixes

---

## LESSON 21 — Basic Shell

### Goal
Buffer keyboard input and respond to commands.

### Keyboard Buffer & readline — `libc/io.c`
```c
#include "io.h"
#include "../drivers/keyboard.h"

// Called from keyboard driver when Enter is pressed
void user_input(char *input) {
    if (strcmp(input, "END") == 0) {
        kprint("Stopping the CPU. Bye!\n");
        asm volatile("hlt");
    }
    kprint("Unknown command: ");
    kprint(input);
    kprint("\n> ");   // re-print prompt
}
```

### Keyboard callback with backspace support — `drivers/keyboard.c`
```c
static void keyboard_callback(registers_t *regs) {
    uint8_t scancode = port_byte_in(0x60);
    char str[2];

    if (scancode == BACKSPACE) {
        if (key_buffer[0] != '\0') {
            kprint_backspace();                          // erase char on screen
            key_buffer[strlen(key_buffer) - 1] = '\0';  // remove from buffer
        }
    } else if (scancode == ENTER) {
        kprint("\n");
        user_input(key_buffer);
        key_buffer[0] = '\0';  // clear buffer
    } else if (scancode <= sizeof(key_ascii)) {
        str[0] = key_ascii[scancode];
        str[1] = '\0';
        if (str[0] != 0) {
            strncat(key_buffer, str, sizeof(key_buffer) - strlen(key_buffer) - 1);
            kprint(str);
        }
    }
}
```

### kprint_backspace — `drivers/screen.c`
```c
void kprint_backspace() {
    int offset = get_cursor_offset() - 2;
    int row = offset / (2 * MAX_COLS);
    int col = (offset - (row * 2 * MAX_COLS)) / 2;
    print_char(0x08, col, row, WHITE_ON_BLACK);  // backspace char — doesn't advance cursor
}
```

### String helpers — `libc/string.c`
```c
int strcmp(char *s1, char *s2) {
    while (*s1 && *s1 == *s2) { s1++; s2++; }
    return (int)((unsigned char)*s1 - (unsigned char)*s2);
}

int strlen(char *s) {
    int i = 0;
    while (s[i++] != '\0');
    return i - 1;
}

void strncat(char *dest, char *src, int n) {
    int dest_len = strlen(dest);
    for (int i = 0; i < n && src[i] != '\0'; i++)
        dest[dest_len + i] = src[i];
    dest[dest_len + n] = '\0';
}
```

---

## LESSON 22 — Kernel Memory Allocator (kmalloc)

### Goal
Implement a simple bump allocator for kernel memory.

### `libc/mem.c`
```c
#include "mem.h"
#include "../cpu/types.h"

void memory_copy(uint8_t *source, uint8_t *dest, int nbytes) {
    for (int i = 0; i < nbytes; i++)
        *(dest + i) = *(source + i);
}

void memory_set(uint8_t *dest, uint8_t val, uint32_t len) {
    for (uint32_t i = 0; i < len; i++)
        dest[i] = val;
}

/* Kernel heap starts at 0x10000 (above IVT + BIOS data + boot sector + kernel) */
uint32_t free_mem_addr = 0x10000;

uint32_t kmalloc(uint32_t size, int align, uint32_t *phys_addr) {
    if (align == 1 && (free_mem_addr & 0xFFFFF000)) {
        free_mem_addr &= 0xFFFFF000;
        free_mem_addr += 0x1000;   // next 4KB page boundary
    }
    if (phys_addr)
        *phys_addr = free_mem_addr;

    uint32_t ret = free_mem_addr;
    free_mem_addr += size;
    return ret;
}
```

### Usage
```c
// Allocate aligned page
uint32_t phys;
uint32_t addr = kmalloc(4096, 1, &phys);  // align=1, returns physical addr

// Simple allocation
uint32_t buf = kmalloc(64, 0, 0);
```

---

## LESSON 23 — Bug Fixes (IMPORTANT — apply all of these)

### Fix 1: CFLAGS
```makefile
# Remove -nostdinc and change -nostdlib approach
CFLAGS = -m32 -g -ffreestanding -Wall -Wextra -fno-pie
# Link with: -ffreestanding -lgcc (not -nostdlib)
```

### Fix 2: Rename main() to kernel_main()
```c
// kernel/kernel.c — change main() to kernel_main()
void kernel_main() {
    ...
}
```
```nasm
; boot/kernel_entry.asm
[extern kernel_main]
global _start
_start:
    call kernel_main
    jmp $
```

### Fix 3: Use stdint.h types (C99 standard)
```c
// Remove cpu/types.h custom types
// Instead in each file:
#include <stdint.h>    // uint8_t, uint16_t, uint32_t
#include <stddef.h>    // size_t, NULL
```
Remove old `u8`, `u16`, `u32` definitions and replace everywhere.

### Fix 4: kmalloc alignment fix
```c
// Use size_t for size parameter
uint32_t kmalloc(size_t size, int align, uint32_t *phys_addr) {
    // Always return page-aligned memory for now
    if (free_mem_addr & 0xFFFFF000) {
        free_mem_addr &= 0xFFFFF000;
        free_mem_addr += 0x1000;
    }
    ...
}
```

### Fix 5: Interrupt registers struct rename
```c
// cpu/isr.h — fix field names
typedef struct {
    uint32_t ds;
    uint32_t edi, esi, ebp, useless, ebx, edx, ecx, eax;  // from pusha
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags, esp, ss;  // pushed by CPU on interrupt
} registers_t;
```

### Fix 6: Interrupt handlers — pass pointer, not value
```nasm
; cpu/interrupt.asm — both common stubs
push esp            ; push pointer to registers struct
cld
call isr_handler    ; C function receives registers_t*
pop eax             ; clean up pointer
```
```c
// cpu/isr.h
typedef void (*isr_t)(registers_t*);
void isr_handler(registers_t *r);    // receives POINTER now
void irq_handler(registers_t *r);
```

### Fix 7: Remove redundant cli/sti in interrupt handlers
```nasm
; Remove 'cli' and 'sti' from interrupt stubs
; iret restores eflags automatically (which includes interrupt flag)
```

## Common Problems & Solutions

| Problem | Solution |
|---------|----------|
| `gcc` warning: implicit function declaration | Add proper `#include` headers; use `#ifndef` guards in `.h` files |
| Keyboard buffer overflow | Check `strlen(key_buffer) < sizeof(key_buffer) - 1` before appending |
| `kmalloc` returns unaligned address | Missing alignment check — ensure `free_mem_addr & 0xFFF == 0` before return |
| Shell doesn't respond to commands | `user_input` not called — check keyboard callback calls it on Enter |
| Linker: cannot find `_start` | Add `global _start` + `_start:` label in `kernel_entry.asm` |
| Crashes after ISR but was working before | Fix 6 — ISR handlers must receive `registers_t *r` (pointer), not value |
| `uint32_t` undefined | Add `#include <stdint.h>` at top of file |
| `size_t` undefined | Add `#include <stddef.h>` |

## Rules to Remember
- `kernel_main` not `main` — gcc treats "main" as special
- Always use `<stdint.h>` types (`uint8_t`, `uint32_t`) not custom ones
- `kmalloc` with `align=1` always page-aligns (4KB = `0x1000`)
- ISR/IRQ handlers take `registers_t *r` (pointer) — ABI requirement
- `iret` restores flags automatically — no need for `sti` in handler
- Keyboard buffer: always null-terminate, always check bounds
