#include "idt.h"
#include "isr.h"

static idt_entry_t    idt[IDT_ENTRIES];
static idt_descriptor_t idt_desc;

extern void isr0();  extern void isr1();  extern void isr2();
extern void isr3();  extern void isr4();  extern void isr5();
extern void isr6();  extern void isr7();  extern void isr8();
extern void isr9();  extern void isr10(); extern void isr11();
extern void isr12(); extern void isr13(); extern void isr14();
extern void isr15(); extern void isr16(); extern void isr17();
extern void isr18(); extern void isr19(); extern void isr20();
extern void isr21(); extern void isr22(); extern void isr23();
extern void isr24(); extern void isr25(); extern void isr26();
extern void isr27(); extern void isr28(); extern void isr29();
extern void isr30(); extern void isr31();

extern void irq0();  extern void irq1();  extern void irq2();
extern void irq3();  extern void irq4();  extern void irq5();
extern void irq6();  extern void irq7();  extern void irq8();
extern void irq9();  extern void irq10(); extern void irq11();
extern void irq12(); extern void irq13(); extern void irq14();
extern void irq15();

void idt_set_gate(uint8_t vector, uint64_t handler, uint8_t type_attr) {
    idt[vector].offset_low  = handler & 0xFFFF;
    idt[vector].selector    = 0x08;
    idt[vector].ist         = 0;
    idt[vector].type_attr   = type_attr;
    idt[vector].offset_mid  = (handler >> 16) & 0xFFFF;
    idt[vector].offset_high = (handler >> 32) & 0xFFFFFFFF;
    idt[vector].reserved    = 0;
}

void idt_init() {

    idt_set_gate(0,  (uint64_t)isr0,  IDT_INTERRUPT_GATE);
    idt_set_gate(1,  (uint64_t)isr1,  IDT_INTERRUPT_GATE);
    idt_set_gate(2,  (uint64_t)isr2,  IDT_INTERRUPT_GATE);
    idt_set_gate(3,  (uint64_t)isr3,  IDT_TRAP_GATE);
    idt_set_gate(4,  (uint64_t)isr4,  IDT_INTERRUPT_GATE);
    idt_set_gate(5,  (uint64_t)isr5,  IDT_INTERRUPT_GATE);
    idt_set_gate(6,  (uint64_t)isr6,  IDT_INTERRUPT_GATE);
    idt_set_gate(7,  (uint64_t)isr7,  IDT_INTERRUPT_GATE);
    idt_set_gate(8,  (uint64_t)isr8,  IDT_INTERRUPT_GATE);
    idt_set_gate(9,  (uint64_t)isr9,  IDT_INTERRUPT_GATE);
    idt_set_gate(10, (uint64_t)isr10, IDT_INTERRUPT_GATE);
    idt_set_gate(11, (uint64_t)isr11, IDT_INTERRUPT_GATE);
    idt_set_gate(12, (uint64_t)isr12, IDT_INTERRUPT_GATE);
    idt_set_gate(13, (uint64_t)isr13, IDT_INTERRUPT_GATE);
    idt_set_gate(14, (uint64_t)isr14, IDT_INTERRUPT_GATE);
    idt_set_gate(15, (uint64_t)isr15, IDT_INTERRUPT_GATE);
    idt_set_gate(16, (uint64_t)isr16, IDT_INTERRUPT_GATE);
    idt_set_gate(17, (uint64_t)isr17, IDT_INTERRUPT_GATE);
    idt_set_gate(18, (uint64_t)isr18, IDT_INTERRUPT_GATE);
    idt_set_gate(19, (uint64_t)isr19, IDT_INTERRUPT_GATE);
    idt_set_gate(20, (uint64_t)isr20, IDT_INTERRUPT_GATE);
    idt_set_gate(21, (uint64_t)isr21, IDT_INTERRUPT_GATE);
    idt_set_gate(22, (uint64_t)isr22, IDT_INTERRUPT_GATE);
    idt_set_gate(23, (uint64_t)isr23, IDT_INTERRUPT_GATE);
    idt_set_gate(24, (uint64_t)isr24, IDT_INTERRUPT_GATE);
    idt_set_gate(25, (uint64_t)isr25, IDT_INTERRUPT_GATE);
    idt_set_gate(26, (uint64_t)isr26, IDT_INTERRUPT_GATE);
    idt_set_gate(27, (uint64_t)isr27, IDT_INTERRUPT_GATE);
    idt_set_gate(28, (uint64_t)isr28, IDT_INTERRUPT_GATE);
    idt_set_gate(29, (uint64_t)isr29, IDT_INTERRUPT_GATE);
    idt_set_gate(30, (uint64_t)isr30, IDT_INTERRUPT_GATE);
    idt_set_gate(31, (uint64_t)isr31, IDT_INTERRUPT_GATE);


    idt_set_gate(32, (uint64_t)irq0,  IDT_INTERRUPT_GATE);
    idt_set_gate(33, (uint64_t)irq1,  IDT_INTERRUPT_GATE);
    idt_set_gate(34, (uint64_t)irq2,  IDT_INTERRUPT_GATE);
    idt_set_gate(35, (uint64_t)irq3,  IDT_INTERRUPT_GATE);
    idt_set_gate(36, (uint64_t)irq4,  IDT_INTERRUPT_GATE);
    idt_set_gate(37, (uint64_t)irq5,  IDT_INTERRUPT_GATE);
    idt_set_gate(38, (uint64_t)irq6,  IDT_INTERRUPT_GATE);
    idt_set_gate(39, (uint64_t)irq7,  IDT_INTERRUPT_GATE);
    idt_set_gate(40, (uint64_t)irq8,  IDT_INTERRUPT_GATE);
    idt_set_gate(41, (uint64_t)irq9,  IDT_INTERRUPT_GATE);
    idt_set_gate(42, (uint64_t)irq10, IDT_INTERRUPT_GATE);
    idt_set_gate(43, (uint64_t)irq11, IDT_INTERRUPT_GATE);
    idt_set_gate(44, (uint64_t)irq12, IDT_INTERRUPT_GATE);
    idt_set_gate(45, (uint64_t)irq13, IDT_INTERRUPT_GATE);
    idt_set_gate(46, (uint64_t)irq14, IDT_INTERRUPT_GATE);
    idt_set_gate(47, (uint64_t)irq15, IDT_INTERRUPT_GATE);


    idt_desc.limit = (sizeof(idt_entry_t) * IDT_ENTRIES) - 1;
    idt_desc.base  = (uint64_t)&idt;

    __asm__ volatile("lidt %0" : : "m"(idt_desc));
}
