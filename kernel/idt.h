#ifndef IDT_H
#define IDT_H

#include "../include/types.h"

#define IDT_ENTRIES 256

#define IDT_INTERRUPT_GATE 0x8E
#define IDT_TRAP_GATE      0x8F
#define IDT_USER_GATE      0xEE

typedef struct {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t reserved;
} __attribute__((packed)) idt_entry_t;

typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) idt_descriptor_t;

typedef struct {
    uint64_t r15, r14, r13, r12;
    uint64_t r11, r10, r9,  r8;
    uint64_t rbp, rdi, rsi, rdx;
    uint64_t rcx, rbx, rax;
    uint64_t int_no;
    uint64_t error_code;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} __attribute__((packed)) registers_t;

void idt_init();
void idt_set_gate(uint8_t vector, uint64_t handler, uint8_t type_attr);

#endif
