#ifndef IDT_H
#define IDT_H

#include "../include/types.h"

// Total IDT entries: 256 possible interrupt vectors
#define IDT_ENTRIES 256

// Gate types
#define IDT_INTERRUPT_GATE 0x8E  // Present, Ring0, 64-bit interrupt gate
#define IDT_TRAP_GATE      0x8F  // Present, Ring0, 64-bit trap gate
#define IDT_USER_GATE      0xEE  // Present, Ring3, 64-bit interrupt gate

// ─────────────────────────────────────────────────
// IDT Entry — 16 bytes in 64-bit mode
// ─────────────────────────────────────────────────
typedef struct {
    uint16_t offset_low;    // Handler address bits  0-15
    uint16_t selector;      // Code segment selector (0x08 = kernel code)
    uint8_t  ist;           // Interrupt Stack Table offset (0 = none)
    uint8_t  type_attr;     // Type and attributes
    uint16_t offset_mid;    // Handler address bits 16-31
    uint32_t offset_high;   // Handler address bits 32-63
    uint32_t reserved;      // Must be zero
} __attribute__((packed)) idt_entry_t;

// ─────────────────────────────────────────────────
// IDT Descriptor — loaded with LIDT instruction
// ─────────────────────────────────────────────────
typedef struct {
    uint16_t limit;         // IDT size in bytes - 1
    uint64_t base;          // Address of IDT array
} __attribute__((packed)) idt_descriptor_t;

// ─────────────────────────────────────────────────
// CPU register state pushed by ISR stubs
// Used by C interrupt handlers
// ─────────────────────────────────────────────────
typedef struct {
    uint64_t r15, r14, r13, r12;
    uint64_t r11, r10, r9,  r8;
    uint64_t rbp, rdi, rsi, rdx;
    uint64_t rcx, rbx, rax;
    uint64_t int_no;        // Interrupt number
    uint64_t error_code;    // Error code (0 if none)
    uint64_t rip;           // Instruction pointer at time of interrupt
    uint64_t cs;            // Code segment
    uint64_t rflags;        // CPU flags
    uint64_t rsp;           // Stack pointer
    uint64_t ss;            // Stack segment
} __attribute__((packed)) registers_t;

void idt_init();
void idt_set_gate(uint8_t vector, uint64_t handler, uint8_t type_attr);

#endif
