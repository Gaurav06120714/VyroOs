#include "isr.h"
#include "../drivers/screen.h"
#include "../drivers/pic.h"

static irq_handler_t irq_handlers[16] = {0};

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


    __asm__ volatile("cli; hlt");
    while (1) {}
}

void irq_handler(registers_t* regs) {
    uint8_t irq = (uint8_t)(regs->int_no - 32);


    if (irq < 16 && irq_handlers[irq]) {
        irq_handlers[irq](regs);
    }


    pic_send_eoi(irq);
}

void irq_register(uint8_t irq, irq_handler_t handler) {
    if (irq < 16) {
        irq_handlers[irq] = handler;
    }
}
