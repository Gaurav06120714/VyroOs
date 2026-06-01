#include "screen.h"

// Internal state
static uint8_t  cursor_row   = 0;
static uint8_t  cursor_col   = 0;
static uint8_t  current_color = WHITE_ON_BLACK;
static uint16_t* vga = (uint16_t*) VGA_ADDRESS;

// ─────────────────────────────────────────────────
// Internal: write one character cell to VGA memory
// ─────────────────────────────────────────────────
static void vga_write(uint8_t row, uint8_t col, char c, uint8_t color) {
    uint32_t index = row * VGA_COLS + col;
    vga[index] = (uint16_t)c | ((uint16_t)color << 8);
}

// ─────────────────────────────────────────────────
// Internal: scroll screen up by one row
// ─────────────────────────────────────────────────
static void scroll() {
    for (uint8_t row = 1; row < VGA_ROWS; row++) {
        for (uint8_t col = 0; col < VGA_COLS; col++) {
            vga[(row - 1) * VGA_COLS + col] = vga[row * VGA_COLS + col];
        }
    }
    // Clear last row
    for (uint8_t col = 0; col < VGA_COLS; col++) {
        vga_write(VGA_ROWS - 1, col, ' ', current_color);
    }
    cursor_row = VGA_ROWS - 1;
    cursor_col = 0;
}

// ─────────────────────────────────────────────────
// screen_init: clear screen and reset cursor
// ─────────────────────────────────────────────────
void screen_init() {
    screen_clear(WHITE_ON_BLACK);
}

// ─────────────────────────────────────────────────
// screen_clear: fill entire screen with spaces
// ─────────────────────────────────────────────────
void screen_clear(uint8_t color) {
    current_color = color;
    for (uint8_t row = 0; row < VGA_ROWS; row++) {
        for (uint8_t col = 0; col < VGA_COLS; col++) {
            vga_write(row, col, ' ', color);
        }
    }
    cursor_row = 0;
    cursor_col = 0;
}

// ─────────────────────────────────────────────────
// screen_set_color: change current text color
// ─────────────────────────────────────────────────
void screen_set_color(uint8_t color) {
    current_color = color;
}

// ─────────────────────────────────────────────────
// set_cursor: move logical cursor position
// ─────────────────────────────────────────────────
void set_cursor(uint8_t row, uint8_t col) {
    cursor_row = row;
    cursor_col = col;
}

// ─────────────────────────────────────────────────
// print_char: write one character, handle newline
// ─────────────────────────────────────────────────
void print_char(char c) {
    if (c == '\n') {
        cursor_col = 0;
        cursor_row++;
    } else if (c == '\r') {
        cursor_col = 0;
    } else if (c == '\t') {
        cursor_col = (cursor_col + 8) & ~7;
    } else {
        vga_write(cursor_row, cursor_col, c, current_color);
        cursor_col++;
    }

    if (cursor_col >= VGA_COLS) {
        cursor_col = 0;
        cursor_row++;
    }

    if (cursor_row >= VGA_ROWS) {
        scroll();
    }
}

// ─────────────────────────────────────────────────
// print: write a null-terminated string
// ─────────────────────────────────────────────────
void print(const char* str) {
    for (int i = 0; str[i] != '\0'; i++) {
        print_char(str[i]);
    }
}

// ─────────────────────────────────────────────────
// print_color: print string in a specific color
// ─────────────────────────────────────────────────
void print_color(const char* str, uint8_t color) {
    uint8_t saved = current_color;
    current_color = color;
    print(str);
    current_color = saved;
}

// ─────────────────────────────────────────────────
// print_line: print string followed by newline
// ─────────────────────────────────────────────────
void print_line(const char* str) {
    print(str);
    print_char('\n');
}

// ─────────────────────────────────────────────────
// print_int: print a 64-bit unsigned integer
// ─────────────────────────────────────────────────
void print_int(uint64_t n) {
    if (n == 0) {
        print_char('0');
        return;
    }
    char buf[20];
    int i = 0;
    while (n > 0) {
        buf[i++] = '0' + (n % 10);
        n /= 10;
    }
    for (int j = i - 1; j >= 0; j--) {
        print_char(buf[j]);
    }
}

// ─────────────────────────────────────────────────
// print_hex: print a 64-bit number in hex
// ─────────────────────────────────────────────────
void print_hex(uint64_t n) {
    const char* hex = "0123456789ABCDEF";
    print("0x");
    int started = 0;
    for (int i = 60; i >= 0; i -= 4) {
        uint8_t nibble = (n >> i) & 0xF;
        if (nibble || started || i == 0) {
            print_char(hex[nibble]);
            started = 1;
        }
    }
}
