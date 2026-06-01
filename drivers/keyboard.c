#include "keyboard.h"
#include "pic.h"
#include "screen.h"
#include "../kernel/isr.h"

// ─────────────────────────────────────────────────
// Scan code → ASCII map (Set 1, lowercase)
// Index = scan code, value = ASCII character
// 0x00 = no mapping (special/modifier key)
// ─────────────────────────────────────────────────
static const char scancode_ascii_lower[] = {
    0,    0,   '1', '2', '3', '4', '5', '6',   // 0x00-0x07
    '7', '8', '9', '0', '-', '=',  0,   '\t',  // 0x08-0x0F
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',   // 0x10-0x17
    'o', 'p', '[', ']', '\n',  0,  'a', 's',   // 0x18-0x1F
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',   // 0x20-0x27
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
static uint8_t  kb_count     = 0;

// Keyboard state
static uint8_t shift_held  = 0;
static uint8_t caps_lock   = 0;

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

    // Scan codes >= 0x80 are key RELEASE events
    if (scancode & 0x80) {
        uint8_t release = scancode & 0x7F;
        // Track shift key release
        if (release == 0x2A || release == 0x36) {
            shift_held = 0;
        }
        return;  // Ignore all other releases
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
// keyboard_init: register IRQ1 handler and unmask
// ─────────────────────────────────────────────────
void keyboard_init() {
    irq_register(1, keyboard_handler);   // IRQ1 = keyboard
    pic_unmask_irq(1);                   // Enable keyboard interrupts
}
