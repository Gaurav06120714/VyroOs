#ifndef PIC_H
#define PIC_H

#include "../include/types.h"

// PIC I/O ports
#define PIC1_COMMAND  0x20    // Master PIC command port
#define PIC1_DATA     0x21    // Master PIC data port
#define PIC2_COMMAND  0xA0    // Slave PIC command port
#define PIC2_DATA     0xA1    // Slave PIC data port

// PIC commands
#define PIC_EOI       0x20    // End-of-interrupt command

// Remap offsets — IRQs 0-15 mapped to vectors 32-47
#define PIC1_OFFSET   0x20    // Master: vectors 32-39
#define PIC2_OFFSET   0x28    // Slave:  vectors 40-47

// IRQ numbers (relative to their PIC)
#define IRQ_TIMER     0
#define IRQ_KEYBOARD  1
#define IRQ_CASCADE   2       // Slave PIC connection
#define IRQ_COM2      3
#define IRQ_COM1      4
#define IRQ_LPT2      5
#define IRQ_FLOPPY    6
#define IRQ_LPT1      7
#define IRQ_RTC       8
#define IRQ_MOUSE     12
#define IRQ_FPU       13
#define IRQ_ATA1      14
#define IRQ_ATA2      15

void pic_init();
void pic_send_eoi(uint8_t irq);
void pic_mask_irq(uint8_t irq);
void pic_unmask_irq(uint8_t irq);

// Low-level port I/O
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void io_wait() {
    outb(0x80, 0);  // Write to unused port — ~1-4 microsecond delay
}

#endif
