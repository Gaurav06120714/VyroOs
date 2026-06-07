#include "isr.h"
#include "../drivers/screen.h"
#include "../drivers/pic.h"

// Table of registered IRQ handlers (one per IRQ line)
static irq_handler_t irq_handlers[16] = {0};

// ─────────────────────────────────────────────────
// isr_handler: called for all CPU exceptions (0-31)
// Prints a kernel panic screen and halts
// ─────────────────────────────────────────────────
void isr_handler(registers_t* regs) {
    screen_set_color(MAKE_COLOR(COLOR_WHITE, COLOR_RED));
    screen_clear(MAKE_COLOR(COLOR_WHITE, COLOR_RED));

    print_color("\n  *** VYRO OS KERNEL PANIC ***\n\n",
                MAKE_COLOR(COLOR_YELLOW, COLOR_RED));

    print("  Exception: ");
    if (regs->int_no < 32) {
        print(exception_messages[regs->int_no]);
    } else {
        print("Unknown");
    }
    print("\n");

    print("  Vector:     "); print_int(regs->int_no);    print("\n");
    print("  Error Code: "); print_hex(regs->error_code); print("\n");
    print("  RIP:        "); print_hex(regs->rip);        print("\n");
    print("  RSP:        "); print_hex(regs->rsp);        print("\n");
    print("  RFLAGS:     "); print_hex(regs->rflags);     print("\n");

    // vC.6.11: capture CR2 on page faults so we can see the address that
    // caused the not-present / protection violation, plus dump the
    // register-window we need to map back to source instructions.
    if (regs->int_no == 14) {
        uint64_t cr2;
        __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
        print("  CR2 (fault address): "); print_hex(cr2); print("\n");
    }

    print("  RAX:        "); print_hex(regs->rax);        print("\n");
    print("  RBX:        "); print_hex(regs->rbx);        print("\n");
    print("  RCX:        "); print_hex(regs->rcx);        print("\n");
    print("  RDX:        "); print_hex(regs->rdx);        print("\n");
    print("  RSI:        "); print_hex(regs->rsi);        print("\n");
    print("  RDI:        "); print_hex(regs->rdi);        print("\n");
    print("  R8:         "); print_hex(regs->r8);         print("\n");
    print("  R10:        "); print_hex(regs->r10);        print("\n");
    print("  R11:        "); print_hex(regs->r11);        print("\n");

    print("\n  System halted. This exception is unrecoverable.\n");

    // Halt permanently
    __asm__ volatile("cli; hlt");
    while (1) {}
}

// ─────────────────────────────────────────────────
// irq_handler: called for all hardware IRQs (32-47)
// Dispatches to registered handler, sends EOI
// ─────────────────────────────────────────────────
void irq_handler(registers_t* regs) {
    uint8_t irq = (uint8_t)(regs->int_no - 32);

    // Call registered handler if one exists
    if (irq < 16 && irq_handlers[irq]) {
        irq_handlers[irq](regs);
    }

    // Always send End-of-Interrupt to PIC
    pic_send_eoi(irq);
}

// ─────────────────────────────────────────────────
// irq_register: install a C handler for a hardware IRQ
// ─────────────────────────────────────────────────
void irq_register(uint8_t irq, irq_handler_t handler) {
    if (irq < 16) {
        irq_handlers[irq] = handler;
    }
}
