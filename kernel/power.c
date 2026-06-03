#include "power.h"
#include "../drivers/pic.h"   // outb/inb

static inline void outw_p(uint16_t port, uint16_t val) {
    __asm__ volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}

// ─────────────────────────────────────────────────
// power_shutdown: ACPI power-off via the QEMU/Bochs ports
// ─────────────────────────────────────────────────
void power_shutdown() {
    // Newer QEMU (>= 2.0): ACPI PM1a control at 0x604
    outw_p(0x604, 0x2000);
    // Older QEMU: 0xB004
    outw_p(0xB004, 0x2000);
    // Bochs / VirtualBox legacy
    outw_p(0x4004, 0x3400);

    // If still alive, halt forever
    __asm__ volatile("cli");
    for (;;) __asm__ volatile("hlt");
}

// ─────────────────────────────────────────────────
// power_reboot: pulse the 8042 reset line
// ─────────────────────────────────────────────────
void power_reboot() {
    // Wait for keyboard controller input buffer to clear
    uint8_t status;
    do { status = inb(0x64); } while (status & 0x02);

    outb(0x64, 0xFE);   // pulse reset line

    // Fallback: triple fault by loading a null IDT and interrupting
    __asm__ volatile("cli");
    for (;;) __asm__ volatile("hlt");
}

// Halt all CPUs. APs sit in their hlt loop already; BSP joins them.
// Doesn't power off the machine — leaves the framebuffer intact so a "halted"
// banner can stay on screen.
void power_halt() {
    __asm__ volatile("cli");
    for (;;) __asm__ volatile("hlt");
}

// Suspend-to-RAM placeholder. Real S3 sleep requires writing SLP_TYPa to the
// PM1a control with bit 13 set; that's its own deeper integration with ACPI
// table parsing. For now we just go to halt as a documented no-op.
void power_suspend() {
    power_halt();
}
