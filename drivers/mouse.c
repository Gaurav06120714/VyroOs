#include "mouse.h"
#include "pic.h"
#include "../kernel/isr.h"
#include "framebuffer.h"

static int      pos_x = FB_WIDTH / 2;
static int      pos_y = FB_HEIGHT / 2;
static uint8_t  buttons = 0;

static uint8_t  packet[3];
static uint8_t  cycle = 0;

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
    outb(0x64, 0xD4);
    mouse_wait_write();
    outb(0x60, val);
}

static uint8_t mouse_read() {
    mouse_wait_read();
    return inb(0x60);
}

void mouse_handler(registers_t* regs) {
    (void)regs;
    uint8_t data = inb(0x60);

    switch (cycle) {
        case 0:

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


            int dx = packet[1];
            int dy = packet[2];
            if (packet[0] & 0x10) dx |= 0xFFFFFF00;
            if (packet[0] & 0x20) dy |= 0xFFFFFF00;

            pos_x += dx;
            pos_y -= dy;

            if (pos_x < 0) pos_x = 0;
            if (pos_y < 0) pos_y = 0;
            if (pos_x >= FB_WIDTH)  pos_x = FB_WIDTH - 1;
            if (pos_y >= FB_HEIGHT) pos_y = FB_HEIGHT - 1;
            break;
        }
    }
}

void mouse_init() {
    /* PS/2 mouse via IRQ12 — present and reliable in every QEMU/VM config.
     * (The VMware vmmouse backdoor is deliberately not used: QEMU's vmport
     * answers its version probe even when no vmmouse device exists, which
     * would switch us to an absolute device that never delivers data and
     * leave the cursor frozen.) */

    mouse_wait_write();
    outb(0x64, 0xA8);                 /* enable aux (mouse) port */


    mouse_wait_write();
    outb(0x64, 0x20);                 /* read controller config byte */
    mouse_wait_read();
    uint8_t status = inb(0x60) | 2;   /* set bit 1: enable mouse IRQ12 */
    status &= ~0x20;                  /* clear bit 5: enable mouse clock */
    mouse_wait_write();
    outb(0x64, 0x60);                 /* write controller config byte */
    mouse_wait_write();
    outb(0x60, status);


    mouse_write(0xF6); mouse_read();  /* set defaults */
    mouse_write(0xF4); mouse_read();  /* enable data reporting */


    irq_register(12, mouse_handler);
    pic_unmask_irq(2);
    pic_unmask_irq(12);
}

void mouse_poll(void) {
    /* PS/2 mouse is interrupt-driven (IRQ12); nothing to poll. */
}

int     mouse_x()       { return pos_x; }
int     mouse_y()       { return pos_y; }
uint8_t mouse_buttons() { return buttons; }
