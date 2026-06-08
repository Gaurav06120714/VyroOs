#include "keyboard.h"
#include "pic.h"
#include "screen.h"
#include "../kernel/isr.h"

static const char scancode_ascii_lower[] = {
    0,    0,   '1', '2', '3', '4', '5', '6',
    '7', '8', '9', '0', '-', '=',  0,   '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',
    'o', 'p', '[', ']', '\n',  0,  'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
    '\'', '`', 0,  '\\','z', 'x', 'c', 'v',   // 0x28-0x2F
    'b', 'n', 'm', ',', '.', '/',   0,  '*',   // 0x30-0x37
     0,  ' ',   0,   0,   0,   0,   0,   0,    // 0x38-0x3F
     0,   0,   0,   0,   0,   0,   0,  '7',    // 0x40-0x47
    '8', '9', '-', '4', '5', '6', '+', '1',   // 0x48-0x4F
    '2', '3', '0', '.',  0,   0,   0,   0,    // 0x50-0x57
};

// Scan code → ASCII map (shifted / uppercase)
static const char scancode_ascii_upper[] = {
    0,    0,   '!', '@', '#', '$', '%', '^',   // 0x00-0x07
    '&', '*', '(', ')', '_', '+',  0,  '\t',   // 0x08-0x0F
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',   // 0x10-0x17
    'O', 'P', '{', '}', '\n',  0,  'A', 'S',   // 0x18-0x1F
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',   // 0x20-0x27
    '"',  '~', 0,  '|', 'Z', 'X', 'C', 'V',   // 0x28-0x2F
    'B', 'N', 'M', '<', '>', '?',  0,  '*',    // 0x30-0x37
     0,  ' ',   0,   0,   0,   0,   0,   0,    // 0x38-0x3F
};

// ─────────────────────────────────────────────────
// Circular input buffer
// ─────────────────────────────────────────────────
static char     kb_buffer[KB_BUFFER_SIZE];
static uint32_t kb_read_pos  = 0;
static uint32_t kb_write_pos = 0;
static uint32_t kb_count     = 0;

// Keyboard state
static uint8_t shift_held    = 0;
static uint8_t caps_lock     = 0;
static uint8_t extended_mode = 0;   // Set when 0xE0 prefix received

// ─────────────────────────────────────────────────
// buffer_push: add character to circular buffer
// ─────────────────────────────────────────────────
static void buffer_push(char c) {
    if (kb_count < KB_BUFFER_SIZE) {
        kb_buffer[kb_write_pos] = c;
        kb_write_pos = (kb_write_pos + 1) % KB_BUFFER_SIZE;
        kb_count++;
    }
}

// ─────────────────────────────────────────────────
// keyboard_getchar: read one character from buffer
// Returns KEY_NONE (0) if buffer is empty
// ─────────────────────────────────────────────────
char keyboard_getchar() {
    if (kb_count == 0) return KEY_NONE;
    char c = kb_buffer[kb_read_pos];
    kb_read_pos = (kb_read_pos + 1) % KB_BUFFER_SIZE;
    kb_count--;
    return c;
}

// ─────────────────────────────────────────────────
// keyboard_has_input: returns 1 if characters ready
// ─────────────────────────────────────────────────
uint8_t keyboard_has_input() {
    return kb_count > 0;
}

// ─────────────────────────────────────────────────
// keyboard_handler: IRQ1 interrupt handler
// Called every time a key is pressed or released
// ─────────────────────────────────────────────────
void keyboard_handler(registers_t* regs) {
    (void)regs;

    uint8_t scancode = inb(KEYBOARD_DATA_PORT);

    // 0xE0 = extended key prefix (arrow keys, etc.)
    if (scancode == 0xE0) {
        extended_mode = 1;
        return;
    }

    // Scan codes >= 0x80 are key RELEASE events
    if (scancode & 0x80) {
        uint8_t release = scancode & 0x7F;
        if (release == 0x2A || release == 0x36) shift_held = 0;
        extended_mode = 0;
        return;
    }

    // Handle extended (0xE0 prefixed) keys — arrow keys
    if (extended_mode) {
        extended_mode = 0;
        switch (scancode) {
            case 0x48: buffer_push(0x01); return;  // Up    arrow → KEY_UP
            case 0x50: buffer_push(0x02); return;  // Down  arrow → KEY_DOWN
            case 0x4B: buffer_push(0x03); return;  // Left  arrow → KEY_LEFT
            case 0x4D: buffer_push(0x04); return;  // Right arrow → KEY_RIGHT
        }
        return;
    }

    // Track modifier keys (press events)
    if (scancode == 0x2A || scancode == 0x36) {
        shift_held = 1;
        return;
    }

    if (scancode == 0x3A) {
        caps_lock = !caps_lock;
        return;
    }

    // Escape key
    if (scancode == 0x01) {
        buffer_push(KEY_ESCAPE);
        return;
    }

    // Backspace
    if (scancode == 0x0E) {
        buffer_push(KEY_BACKSPACE);
        return;
    }

    // Translate scan code to ASCII
    char c = 0;
    uint8_t table_size = sizeof(scancode_ascii_lower);

    if (scancode < table_size) {
        uint8_t use_upper = shift_held ^ caps_lock;

        if (use_upper && scancode < sizeof(scancode_ascii_upper)) {
            c = scancode_ascii_upper[scancode];
        }
        if (!c) {
            c = scancode_ascii_lower[scancode];
        }
    }

    if (c) {
        buffer_push(c);
    }
}

// ─────────────────────────────────────────────────
// keyboard_init: drain residual i8042 output buffer,
// then register IRQ1 handler and unmask.
//
// The i8042 controller latches whatever byte was in
// its output register at power-on (often a self-test
// result, a stale scan code, or 0xAA).  If we don't
// drain it before unmasking IRQ1 the very first real
// keypress fires IRQ1 twice — once for the residual
// byte and once for the real scan code — so the first
// character the user types is silently swallowed.
// Draining: poll bit 0 of status port (OBF = output
// buffer full) and discard bytes until the controller
// reports empty.  Up to 16 bytes to handle stuck
// multi-byte sequences; bail early once OBF is clear.
// ─────────────────────────────────────────────────
void keyboard_init() {
    // Drain any stale bytes sitting in the i8042 output buffer
    for (int i = 0; i < 16; i++) {
        if (!(inb(KEYBOARD_STATUS_PORT) & 0x01)) break;
        (void)inb(KEYBOARD_DATA_PORT);   // discard the byte
    }

    irq_register(1, keyboard_handler);   // IRQ1 = keyboard
    pic_unmask_irq(1);                   // Enable keyboard interrupts
}
