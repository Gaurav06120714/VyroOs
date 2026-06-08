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
// VMware/QEMU backdoor absolute mouse interface.
// Port 0x5658, magic number 0x564D5868 ("VMXh").
// QEMU's usb-tablet device exposes this protocol so
// we can read absolute screen coordinates and button
// state without relying on PS/2 relative packets —
// which the usb-tablet doesn't generate.
//
// Commands used:
//   0x27 ABSPTR_COMMAND  — enable/disable abs mode
//   0x28 ABSPTR_STATUS   — count of pending samples
//   0x29 ABSPTR_DATA     — read one X/Y/Z/buttons sample
//
// Reference: VMware backdoor protocol, QEMU hw/input/vmmouse.c
// ─────────────────────────────────────────────────
#define VMMOUSE_MAGIC     0x564D5868UL
#define VMMOUSE_PORT      0x5658
#define VMMOUSE_CMD_ABSPTR_COMMAND  0x27
#define VMMOUSE_CMD_ABSPTR_STATUS   0x28
#define VMMOUSE_CMD_ABSPTR_DATA     0x29
#define VMMOUSE_ABSPTR_ENABLE       0x45414552UL

static int vmmouse_active = 0;   // 1 once the backdoor ACKed ENABLE

// Issue one VMware backdoor command.
// Returns EBX (used for status/data word 0).
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

// Try to enable VMware absolute mouse mode.
// Returns 1 if the backdoor is present and accepted the command.
static int vmmouse_enable(void) {
    uint32_t eax = 0, ecx = 0, edx = 0;
    vmmouse_cmd(VMMOUSE_CMD_ABSPTR_COMMAND, VMMOUSE_ABSPTR_ENABLE,
                &eax, &ecx, &edx);
    // QEMU sets EAX to VMMOUSE_MAGIC on ACK
    return (eax == VMMOUSE_MAGIC);
}

// Poll the backdoor for new absolute position+button data.
// Updates pos_x/pos_y/buttons if a sample is available.
// Must be called from non-interrupt context (the GUI render loop).
static void vmmouse_poll(void) {
    // Read number of pending words in the backdoor queue
    uint32_t count = vmmouse_cmd(VMMOUSE_CMD_ABSPTR_STATUS, 0,
                                  0, 0, 0);
    if ((count & 0xFFFF) < 4) return;   // need at least 4 words (X,Y,Z,btn)

    uint32_t x_raw, y_raw, dummy, btn_raw;
    x_raw   = vmmouse_cmd(VMMOUSE_CMD_ABSPTR_DATA, 0, 0, 0, 0);
    y_raw   = vmmouse_cmd(VMMOUSE_CMD_ABSPTR_DATA, 0, 0, 0, 0);
    dummy   = vmmouse_cmd(VMMOUSE_CMD_ABSPTR_DATA, 0, 0, 0, 0);   // Z axis
    btn_raw = vmmouse_cmd(VMMOUSE_CMD_ABSPTR_DATA, 0, 0, 0, 0);
    (void)dummy;

    // X/Y are in the range 0–0xFFFF, scale to screen pixels
    pos_x = (int)((x_raw * FB_WIDTH)  >> 16);
    pos_y = (int)((y_raw * FB_HEIGHT) >> 16);

    if (pos_x < 0) pos_x = 0;
    if (pos_y < 0) pos_y = 0;
    if (pos_x >= FB_WIDTH)  pos_x = FB_WIDTH  - 1;
    if (pos_y >= FB_HEIGHT) pos_y = FB_HEIGHT - 1;

    // Bit 5 = left, bit 4 = right, bit 3 = middle in the QEMU vmmouse data
    uint8_t new_btn = 0;
    if (btn_raw & (1 << 5)) new_btn |= MOUSE_LEFT;
    if (btn_raw & (1 << 4)) new_btn |= MOUSE_RIGHT;
    if (btn_raw & (1 << 3)) new_btn |= MOUSE_MIDDLE;
    buttons = new_btn;
}

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
// mouse_init: try VMware absolute backdoor first;
// fall back to PS/2 relative if not available.
// ─────────────────────────────────────────────────
void mouse_init() {
    // Attempt VMware/QEMU absolute mouse (works with usb-tablet)
    if (vmmouse_enable()) {
        vmmouse_active = 1;
        // No IRQ12 needed for polling mode — we poll in the GUI render loop
        return;
    }

    // Fallback: standard PS/2 auxiliary device
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

// ─────────────────────────────────────────────────
// mouse_poll: call from GUI render loop (not IRQ ctx)
// Pulls fresh absolute samples from the VMware
// backdoor when active; no-op in PS/2 mode (IRQ-driven).
// ─────────────────────────────────────────────────
void mouse_poll(void) {
    if (vmmouse_active) vmmouse_poll();
}

int     mouse_x()       { return pos_x; }
int     mouse_y()       { return pos_y; }
uint8_t mouse_buttons() { return buttons; }
