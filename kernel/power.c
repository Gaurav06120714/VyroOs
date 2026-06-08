#include "power.h"
#include "../drivers/pic.h"

static inline void outw_p(uint16_t port, uint16_t val) {
    __asm__ volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}

void power_shutdown() {

    outw_p(0x604, 0x2000);

    outw_p(0xB004, 0x2000);

    outw_p(0x4004, 0x3400);


    __asm__ volatile("cli");
    for (;;) __asm__ volatile("hlt");
}

void power_reboot() {

    uint8_t status;
    do { status = inb(0x64); } while (status & 0x02);

    outb(0x64, 0xFE);


    __asm__ volatile("cli");
    for (;;) __asm__ volatile("hlt");
}

void power_halt() {
    __asm__ volatile("cli");
    for (;;) __asm__ volatile("hlt");
}

void power_suspend() {
    power_halt();
}
