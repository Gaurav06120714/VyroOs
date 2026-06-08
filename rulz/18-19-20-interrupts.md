# LESSON 18-19-20 — Interrupts, IRQs, Timer & Keyboard
> Sources:
> - https://github.com/cfenollosa/os-tutorial/tree/master/18-interrupts
> - https://github.com/cfenollosa/os-tutorial/tree/master/19-interrupts-irqs
> - https://github.com/cfenollosa/os-tutorial/tree/master/20-interrupts-timer

## Concepts
IDT, ISR, IRQ, PIC remapping, EOI, timer, keyboard scancodes

---

## LESSON 18 — Interrupt Descriptor Table (IDT)

### Goal
Set up the IDT so the CPU can handle exceptions and hardware interrupts.

### Theory
The IDT is a table of 256 entries. Each entry says "when interrupt N fires, jump here."
- ISRs 0–31 = CPU exceptions (divide by zero, page fault, etc.)
- ISRs 32–47 = Hardware IRQs (after PIC remapping)
- ISRs 48–255 = Software interrupts (syscalls, etc.)

### Data Types — `cpu/types.h`
```c
typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;
```

### IDT Entry Structure — `cpu/idt.h`
```c
typedef struct {
    uint16_t low_offset;    // handler address bits 0-15
    uint16_t sel;           // code segment selector (= CODE_SEG = 0x08)
    uint8_t  always0;       // always 0
    uint8_t  flags;         // present, privilege, type flags
    uint16_t high_offset;   // handler address bits 16-31
} __attribute__((packed)) idt_gate_t;

typedef struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) idt_register_t;

#define IDT_ENTRIES 256
extern idt_gate_t idt[IDT_ENTRIES];
extern idt_register_t idt_reg;

void set_idt_gate(int n, uint32_t handler);
void set_idt();
```

### Set IDT Gate — `cpu/idt.c`
```c
#include "idt.h"

idt_gate_t     idt[IDT_ENTRIES];
idt_register_t idt_reg;

void set_idt_gate(int n, uint32_t handler) {
    idt[n].low_offset  = low_16(handler);
    idt[n].sel         = 0x08;   // CODE_SEG
    idt[n].always0     = 0;
    idt[n].flags       = 0x8e;   // present, ring 0, 32-bit interrupt gate
    idt[n].high_offset = high_16(handler);
}

void set_idt() {
    idt_reg.base  = (uint32_t) &idt;
    idt_reg.limit = IDT_ENTRIES * sizeof(idt_gate_t) - 1;
    __asm__ volatile("lidt (%0)" : : "r"(&idt_reg));
}
```

### CPU Exception ISRs — `cpu/interrupt.asm` (low-level stubs)
```nasm
; Common ISR stub — saves state, calls C handler, restores
isr_common_stub:
    pusha
    mov ax, ds
    push eax            ; save data segment

    mov ax, 0x10        ; kernel data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp            ; pass pointer to registers struct
    cld                 ; C calling convention: direction flag clear
    call isr_handler
    pop eax

    pop eax             ; restore original data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    popa
    add esp, 8          ; pop error code and ISR number
    iret

; Generate ISR stubs for exceptions 0-31
; Exceptions WITH error code: 8, 10, 11, 12, 13, 14, 17
%macro ISR_NOERRCODE 1
global isr%1
isr%1:
    push byte 0         ; dummy error code
    push byte %1
    jmp isr_common_stub
%endmacro

%macro ISR_ERRCODE 1
global isr%1
isr%1:
    push byte %1        ; error code already pushed by CPU
    jmp isr_common_stub
%endmacro

ISR_NOERRCODE  0   ; Divide by Zero
ISR_NOERRCODE  1   ; Debug
ISR_NOERRCODE  2   ; NMI
ISR_NOERRCODE  3   ; Breakpoint
ISR_NOERRCODE  4   ; Overflow
ISR_NOERRCODE  5   ; Bound Range Exceeded
ISR_NOERRCODE  6   ; Invalid Opcode
ISR_NOERRCODE  7   ; Device Not Available
ISR_ERRCODE    8   ; Double Fault
ISR_NOERRCODE  9
ISR_ERRCODE   10   ; Invalid TSS
ISR_ERRCODE   11   ; Segment Not Present
ISR_ERRCODE   12   ; Stack-Segment Fault
ISR_ERRCODE   13   ; General Protection Fault
ISR_ERRCODE   14   ; Page Fault
ISR_NOERRCODE 15
ISR_NOERRCODE 16   ; x87 FPU Error
ISR_ERRCODE   17   ; Alignment Check
ISR_NOERRCODE 18   ; Machine Check
ISR_NOERRCODE 19   ; SIMD Floating-Point
```

### High-level ISR handler — `cpu/isr.c`
```c
#include "isr.h"
#include "../drivers/screen.h"

isr_t interrupt_handlers[256];

static char *exception_messages[] = {
    "Division By Zero", "Debug", "Non Maskable Interrupt",
    "Breakpoint", "Into Detected Overflow", "Out of Bounds",
    "Invalid Opcode", "No Coprocessor", "Double Fault",
    "Coprocessor Segment Overrun", "Bad TSS", "Segment Not Present",
    "Stack Fault", "General Protection Fault", "Page Fault",
    "Unknown Interrupt", "Coprocessor Fault", "Alignment Check",
    "Machine Check", "Reserved", ...
};

void isr_handler(registers_t *r) {
    if (r->int_no < 32) {
        kprint(exception_messages[r->int_no]);
        kprint("\nSystem Halted!\n");
        for(;;);    // halt
    }
}

void isr_install() {
    set_idt_gate(0,  (uint32_t)isr0);
    set_idt_gate(1,  (uint32_t)isr1);
    // ... all 32
    set_idt();
    kprint("IDT loaded\n");
}
```

---

## LESSON 19 — IRQs & PIC Remapping

### Problem
At boot, the PIC maps IRQs 0–7 to INT 0x8–0xF, which conflicts with CPU exceptions (ISRs 0–31).

### Solution: Remap PIC
```c
// In isr_install(), after setting exception gates:
void remap_pic() {
    // ICW1: init
    port_byte_out(0x20, 0x11);  // master PIC
    port_byte_out(0xA0, 0x11);  // slave PIC
    // ICW2: vector offsets
    port_byte_out(0x21, 0x20);  // master IRQs → INT 32-39
    port_byte_out(0xA1, 0x28);  // slave IRQs  → INT 40-47
    // ICW3: cascade
    port_byte_out(0x21, 0x04);
    port_byte_out(0xA1, 0x02);
    // ICW4: 8086 mode
    port_byte_out(0x21, 0x01);
    port_byte_out(0xA1, 0x01);
    // Unmask all IRQs
    port_byte_out(0x21, 0x0);
    port_byte_out(0xA1, 0x0);
}
```

### IRQ Handler Stubs — `cpu/interrupt.asm`
```nasm
irq_common_stub:
    pusha
    mov ax, ds
    push eax
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    push esp
    cld
    call irq_handler
    pop ebx
    pop ebx
    mov ds, bx
    mov es, bx
    mov fs, bx
    mov gs, bx
    popa
    add esp, 8
    iret

; IRQ 0-15 (INT 32-47 after remapping)
%macro IRQ 2
global irq%1
irq%1:
    push byte 0
    push byte %2
    jmp irq_common_stub
%endmacro

IRQ  0, 32    ; Timer
IRQ  1, 33    ; Keyboard
IRQ  2, 34
IRQ  3, 35
IRQ  4, 36
IRQ  5, 37
IRQ  6, 38
IRQ  7, 39
IRQ  8, 40
IRQ  9, 41
IRQ 10, 42
IRQ 11, 43
IRQ 12, 44
IRQ 13, 45
IRQ 14, 46
IRQ 15, 47
```

### C IRQ Handler
```c
void irq_handler(registers_t *r) {
    // Send EOI (End of Interrupt) to PIC
    if (r->int_no >= 40)
        port_byte_out(0xA0, 0x20);   // slave EOI
    port_byte_out(0x20, 0x20);       // master EOI

    // Call registered handler
    if (interrupt_handlers[r->int_no] != 0)
        interrupt_handlers[r->int_no](r);
}

void register_interrupt_handler(uint8_t n, isr_t handler) {
    interrupt_handlers[n] = handler;
}
```

---

## LESSON 20 — Timer & Keyboard Drivers

### Timer Driver — `cpu/timer.c`
```c
#include "timer.h"
#include "isr.h"
#include "../cpu/ports.h"

uint32_t tick = 0;

static void timer_callback(registers_t *regs) {
    tick++;
    // kprint tick count here if needed
}

void init_timer(uint32_t freq) {
    register_interrupt_handler(IRQ0, timer_callback);

    uint32_t divisor = 1193180 / freq;   // PIT runs at 1.193180 MHz
    uint8_t  low  = (uint8_t)(divisor & 0xFF);
    uint8_t  high = (uint8_t)((divisor >> 8) & 0xFF);

    port_byte_out(0x43, 0x36);  // PIT command: channel 0, mode 3, binary
    port_byte_out(0x40, low);
    port_byte_out(0x40, high);
}
```

**Usage in `kernel_main`:**
```c
isr_install();   // sets up IDT + ISRs
irq_install();   // remaps PIC, sets up IRQ gates
asm volatile("sti");   // ENABLE interrupts
init_timer(50);  // 50 Hz timer
```

### Keyboard Driver — `drivers/keyboard.c`
```c
#include "keyboard.h"
#include "../cpu/isr.h"
#include "../cpu/ports.h"
#include "screen.h"

// US QWERTY scancode to ASCII mapping (partial)
static char key_ascii[] = {
    0, 0, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,'\\','z','x','c','v','b','n','m',',','.','/', 0, '*',
    0, ' '
};

static char key_buffer[256];

static void keyboard_callback(registers_t *regs) {
    uint8_t scancode = port_byte_in(0x60);  // read scancode from keyboard port

    if (scancode > sizeof(key_ascii)) return;

    if (scancode == BACKSPACE) {
        // handle backspace
        if (key_buffer[0] != 0) {
            kprint_backspace();
            key_buffer[strlen(key_buffer)-1] = 0;
        }
    } else if (scancode == ENTER) {
        kprint("\n");
        user_input(key_buffer);   // pass to shell
        key_buffer[0] = 0;
    } else {
        char letter[2] = {key_ascii[scancode], 0};
        strncat(key_buffer, letter, 1);
        kprint(letter);
    }
}

void init_keyboard() {
    register_interrupt_handler(IRQ1, keyboard_callback);
}
```

## Common Problems & Solutions

| Problem | Solution |
|---------|----------|
| Triple fault when enabling interrupts | IDT not set up before `sti` — call `isr_install()` and `irq_install()` first |
| Keyboard IRQ fires but no character | Check scancode table — only key-down events have scancodes < 0x80; key-up = scancode + 0x80 |
| Timer fires but crashes | Missing EOI — must call `port_byte_out(0x20, 0x20)` at end of every IRQ handler |
| PIC conflicts (INT 8 = double fault = timer) | PIC not remapped — call `remap_pic()` before enabling IRQs |
| `sti` must come AFTER IDT setup | Wrong order — set up IDT, then `sti` |
| ISR fires but `isr_handler` not called | `push esp` / `call isr_handler` issue — pass pointer to registers struct |

## Rules to Remember
- `cli` during IDT setup, `sti` after everything is ready
- Remap PIC: IRQs 0–7 → INT 32–39, IRQs 8–15 → INT 40–47
- ALWAYS send EOI after IRQ handler: `port_byte_out(0x20, 0x20)`
- Keyboard: read scancode from port `0x60`, key-up events have bit 7 set
- Timer: PIT clock = 1,193,180 Hz; divisor = `1193180 / desired_freq`
- Register handler with `register_interrupt_handler(IRQ0, callback)` before `sti`
