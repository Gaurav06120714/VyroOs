#include "pic.h"

// ─────────────────────────────────────────────────
// pic_init: remap PIC IRQs to vectors 32-47
//
// By default the PIC maps:
//   IRQ 0-7  → INT 8-15   (conflicts with CPU exceptions!)
//   IRQ 8-15 → INT 70-77
//
// After remapping:
//   IRQ 0-7  → INT 32-39
//   IRQ 8-15 → INT 40-47
//
// ICW = Initialization Command Word
// ─────────────────────────────────────────────────
void pic_init() {
    // ICW1: Start initialization sequence (cascade mode)
    outb(PIC1_COMMAND, 0x11);   io_wait();
    outb(PIC2_COMMAND, 0x11);   io_wait();

    // ICW2: Set vector offsets
    outb(PIC1_DATA, PIC1_OFFSET);  io_wait();   // Master → 0x20 (32)
    outb(PIC2_DATA, PIC2_OFFSET);  io_wait();   // Slave  → 0x28 (40)

    // ICW3: Tell master there is a slave on IRQ2
    outb(PIC1_DATA, 0x04);      io_wait();   // Bit 2 = IRQ2 has slave
    outb(PIC2_DATA, 0x02);      io_wait();   // Slave cascade identity

    // ICW4: Set 8086 mode
    outb(PIC1_DATA, 0x01);      io_wait();
    outb(PIC2_DATA, 0x01);      io_wait();

    // Mask all IRQs except keyboard (IRQ1) — unmask as needed
    outb(PIC1_DATA, 0xFD);  // 11111101 = all masked except IRQ1
    outb(PIC2_DATA, 0xFF);  // All slave IRQs masked
}

// ─────────────────────────────────────────────────
// pic_send_eoi: signal end-of-interrupt to PIC
// Must be called at end of every IRQ handler
// ─────────────────────────────────────────────────
void pic_send_eoi(uint8_t irq) {
    if (irq >= 8) {
        outb(PIC2_COMMAND, PIC_EOI);  // Slave must also be notified
    }
    outb(PIC1_COMMAND, PIC_EOI);
}

// ─────────────────────────────────────────────────
// pic_mask_irq: disable a specific IRQ line
// ─────────────────────────────────────────────────
void pic_mask_irq(uint8_t irq) {
    uint16_t port;
    uint8_t mask;
    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    mask = inb(port) | (1 << irq);
    outb(port, mask);
}

// ─────────────────────────────────────────────────
// pic_unmask_irq: enable a specific IRQ line
// ─────────────────────────────────────────────────
void pic_unmask_irq(uint8_t irq) {
    uint16_t port;
    uint8_t mask;
    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    mask = inb(port) & ~(1 << irq);
    outb(port, mask);
}
