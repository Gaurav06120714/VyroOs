#include "pic.h"

void pic_init() {

    outb(PIC1_COMMAND, 0x11);   io_wait();
    outb(PIC2_COMMAND, 0x11);   io_wait();


    outb(PIC1_DATA, PIC1_OFFSET);  io_wait();
    outb(PIC2_DATA, PIC2_OFFSET);  io_wait();


    outb(PIC1_DATA, 0x04);      io_wait();
    outb(PIC2_DATA, 0x02);      io_wait();


    outb(PIC1_DATA, 0x01);      io_wait();
    outb(PIC2_DATA, 0x01);      io_wait();


    outb(PIC1_DATA, 0xFD);
    outb(PIC2_DATA, 0xFF);
}

void pic_send_eoi(uint8_t irq) {
    if (irq >= 8) {
        outb(PIC2_COMMAND, PIC_EOI);
    }
    outb(PIC1_COMMAND, PIC_EOI);
}

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
