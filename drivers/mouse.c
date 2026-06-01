#include "mouse.h"
#include "pic.h"
#include "../kernel/isr.h"
#include "framebuffer.h"

// Mouse position (clamped to screen)
static int      pos_x = FB_WIDTH / 2;
static int      pos_y = FB_HEIGHT / 2;
static uint8_t  buttons = 0;

// Packet assembly
static uint8_t  packet[3];
static uint8_t  cycle = 0;

// ─────────────────────────────────────────────────
// PS/2 controller wait helpers
// ─────────────────────────────────────────────────
static void mouse_wait_write() {
    for (int i = 0; i < 100000; i++)
        if ((inb(0x64) & 2) == 0) return;
}
static void mouse_wait_read() {
    for (int i = 0; i < 100000; i++)
        if (inb(0x64) & 1) return;
}

static void mouse_write(uint8_t val) {
    mouse_wait_write();
    outb(0x64, 0xD4);          // tell controller: next byte is for the mouse
    mouse_wait_write();
    outb(0x60, val);
}

static uint8_t mouse_read() {
    mouse_wait_read();
    return inb(0x60);
}

// ─────────────────────────────────────────────────
// mouse_handler: IRQ12 — assemble 3-byte packets
// ─────────────────────────────────────────────────
void mouse_handler(registers_t* regs) {
    (void)regs;
    uint8_t data = inb(0x60);

    switch (cycle) {
        case 0:
            // First byte must have bit 3 set (sync)
            if (!(data & 0x08)) return;
            packet[0] = data;
            cycle = 1;
            break;
        case 1:
            packet[1] = data;
            cycle = 2;
            break;
        case 2: {
            packet[2] = data;
            cycle = 0;

            buttons = packet[0] & 0x07;

            // Signed deltas (9-bit, sign in byte0)
            int dx = packet[1];
            int dy = packet[2];
            if (packet[0] & 0x10) dx |= 0xFFFFFF00;   // sign-extend X
            if (packet[0] & 0x20) dy |= 0xFFFFFF00;   // sign-extend Y

            pos_x += dx;
            pos_y -= dy;   // screen Y grows downward

            if (pos_x < 0) pos_x = 0;
            if (pos_y < 0) pos_y = 0;
            if (pos_x >= FB_WIDTH)  pos_x = FB_WIDTH - 1;
            if (pos_y >= FB_HEIGHT) pos_y = FB_HEIGHT - 1;
            break;
        }
    }
}

// ─────────────────────────────────────────────────
// mouse_init: enable the PS/2 auxiliary device + IRQ12
// ─────────────────────────────────────────────────
void mouse_init() {
    // Enable auxiliary mouse device
    mouse_wait_write();
    outb(0x64, 0xA8);

    // Enable IRQ12: read config byte, set bit 1, write back
    mouse_wait_write();
    outb(0x64, 0x20);
    mouse_wait_read();
    uint8_t status = inb(0x60) | 2;
    mouse_wait_write();
    outb(0x64, 0x60);
    mouse_wait_write();
    outb(0x60, status);

    // Set defaults, then enable packet streaming
    mouse_write(0xF6); mouse_read();
    mouse_write(0xF4); mouse_read();

    // Register IRQ12 handler; unmask cascade (IRQ2) + mouse (IRQ12)
    irq_register(12, mouse_handler);
    pic_unmask_irq(2);
    pic_unmask_irq(12);
}

int     mouse_x()       { return pos_x; }
int     mouse_y()       { return pos_y; }
uint8_t mouse_buttons() { return buttons; }
