#include "mouse.h"
#include "pic.h"
#include "../kernel/isr.h"
#include "framebuffer.h"

static int      pos_x = FB_WIDTH / 2;
static int      pos_y = FB_HEIGHT / 2;
static uint8_t  buttons = 0;

static uint8_t  packet[3];
static uint8_t  cycle = 0;

#define VMMOUSE_MAGIC     0x564D5868UL
#define VMMOUSE_PORT      0x5658
#define VMMOUSE_CMD_ABSPTR_COMMAND  0x27
#define VMMOUSE_CMD_ABSPTR_STATUS   0x28
#define VMMOUSE_CMD_ABSPTR_DATA     0x29
#define VMMOUSE_ABSPTR_ENABLE       0x45414552UL

static int vmmouse_active = 0;

static uint32_t vmmouse_cmd(uint32_t cmd, uint32_t arg,
                             uint32_t* eax_out, uint32_t* ecx_out,
                             uint32_t* edx_out) {
    uint32_t eax = VMMOUSE_MAGIC, ebx = arg, ecx = cmd, edx = VMMOUSE_PORT;
    __asm__ volatile(
        "inl %%dx, %%eax"
        : "+a"(eax), "+b"(ebx), "+c"(ecx), "+d"(edx)
        :
        : "memory"
    );
    if (eax_out) *eax_out = eax;
    if (ecx_out) *ecx_out = ecx;
    if (edx_out) *edx_out = edx;
    return ebx;
}

static int vmmouse_enable(void) {
    uint32_t eax = 0, ecx = 0, edx = 0;
    vmmouse_cmd(VMMOUSE_CMD_ABSPTR_COMMAND, VMMOUSE_ABSPTR_ENABLE,
                &eax, &ecx, &edx);

    return (eax == VMMOUSE_MAGIC);
}

static void vmmouse_poll(void) {

    uint32_t count = vmmouse_cmd(VMMOUSE_CMD_ABSPTR_STATUS, 0,
                                  0, 0, 0);
    if ((count & 0xFFFF) < 4) return;

    uint32_t x_raw, y_raw, dummy, btn_raw;
    x_raw   = vmmouse_cmd(VMMOUSE_CMD_ABSPTR_DATA, 0, 0, 0, 0);
    y_raw   = vmmouse_cmd(VMMOUSE_CMD_ABSPTR_DATA, 0, 0, 0, 0);
    dummy   = vmmouse_cmd(VMMOUSE_CMD_ABSPTR_DATA, 0, 0, 0, 0);
    btn_raw = vmmouse_cmd(VMMOUSE_CMD_ABSPTR_DATA, 0, 0, 0, 0);
    (void)dummy;


    pos_x = (int)((x_raw * FB_WIDTH)  >> 16);
    pos_y = (int)((y_raw * FB_HEIGHT) >> 16);

    if (pos_x < 0) pos_x = 0;
    if (pos_y < 0) pos_y = 0;
    if (pos_x >= FB_WIDTH)  pos_x = FB_WIDTH  - 1;
    if (pos_y >= FB_HEIGHT) pos_y = FB_HEIGHT - 1;


    uint8_t new_btn = 0;
    if (btn_raw & (1 << 5)) new_btn |= MOUSE_LEFT;
    if (btn_raw & (1 << 4)) new_btn |= MOUSE_RIGHT;
    if (btn_raw & (1 << 3)) new_btn |= MOUSE_MIDDLE;
    buttons = new_btn;
}

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

    if (vmmouse_enable()) {
        vmmouse_active = 1;

        return;
    }


    mouse_wait_write();
    outb(0x64, 0xA8);


    mouse_wait_write();
    outb(0x64, 0x20);
    mouse_wait_read();
    uint8_t status = inb(0x60) | 2;
    mouse_wait_write();
    outb(0x64, 0x60);
    mouse_wait_write();
    outb(0x60, status);


    mouse_write(0xF6); mouse_read();
    mouse_write(0xF4); mouse_read();


    irq_register(12, mouse_handler);
    pic_unmask_irq(2);
    pic_unmask_irq(12);
}

void mouse_poll(void) {
    if (vmmouse_active) vmmouse_poll();
}

int     mouse_x()       { return pos_x; }
int     mouse_y()       { return pos_y; }
uint8_t mouse_buttons() { return buttons; }
