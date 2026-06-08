#include "screen.h"
#include "framebuffer.h"

#define VGA ((volatile uint16_t*)VGA_ADDRESS)
static uint8_t cursor_row    = 0;
static uint8_t cursor_col    = 0;
static uint8_t current_color = WHITE_ON_BLACK;

static char    shadow_char[VGA_ROWS][VGA_COLS];
static uint8_t shadow_attr[VGA_ROWS][VGA_COLS];

static const uint32_t color_table[16] = {
    0x000000,
    0x0000AA,
    0x00AA00,
    0x00AAAA,
    0xAA0000,
    0xAA00AA,
    0xAA5500,
    0xAAAAAA,
    0x555555,
    0x5555FF,
    0x55FF55,
    0x55FFFF,
    0xFF5555,
    0xFF55FF,
    0xFFFF55,
    0xFFFFFF,
};

static inline uint32_t vga_fg(uint8_t color) {
    return color_table[color & 0x0F];
}
static inline uint32_t vga_bg(uint8_t color) {
    return color_table[(color >> 4) & 0x0F];
}

static void vga_write(uint8_t row, uint8_t col, char c, uint8_t color) {
    if (row >= VGA_ROWS || col >= VGA_COLS) return;


    shadow_char[row][col] = c;
    shadow_attr[row][col] = color;

    if (fb_available()) {
        fb_putchar(col, row, c, vga_fg(color), vga_bg(color));
    } else {
        if (col >= 80 || row >= 50) return;
        uint32_t index = (uint32_t)row * 80 + col;
        VGA[index] = (uint16_t)c | ((uint16_t)color << 8);
    }
}

static void scroll(void) {

    for (uint8_t row = 1; row < VGA_ROWS; row++) {
        for (uint8_t col = 0; col < VGA_COLS; col++) {
            shadow_char[row - 1][col] = shadow_char[row][col];
            shadow_attr[row - 1][col] = shadow_attr[row][col];
        }
    }

    for (uint8_t col = 0; col < VGA_COLS; col++) {
        shadow_char[VGA_ROWS - 1][col] = ' ';
        shadow_attr[VGA_ROWS - 1][col] = current_color;
    }


    for (uint8_t row = 0; row < VGA_ROWS; row++) {
        for (uint8_t col = 0; col < VGA_COLS; col++) {
            char    ch = shadow_char[row][col];
            uint8_t at = shadow_attr[row][col];
            if (fb_available()) {
                fb_putchar(col, row, ch, vga_fg(at), vga_bg(at));
            } else if (col < 80 && row < 50) {
                VGA[(uint32_t)row * 80 + col] = (uint16_t)ch | ((uint16_t)at << 8);
            }
        }
    }

    cursor_row = VGA_ROWS - 1;
    cursor_col = 0;
}

void screen_init(void) {
    screen_clear(WHITE_ON_BLACK);
}

void screen_clear(uint8_t color) {
    current_color = color;


    for (uint8_t row = 0; row < VGA_ROWS; row++) {
        for (uint8_t col = 0; col < VGA_COLS; col++) {
            shadow_char[row][col] = ' ';
            shadow_attr[row][col] = color;
        }
    }

    if (fb_available()) {
        fb_clear(vga_bg(color));
    } else {
        for (uint8_t row = 0; row < VGA_ROWS && row < 50; row++) {
            for (uint8_t col = 0; col < VGA_COLS && col < 80; col++) {
                uint32_t index = (uint32_t)row * 80 + col;
                VGA[index] = (uint16_t)' ' | ((uint16_t)color << 8);
            }
        }
    }
    cursor_row = 0;
    cursor_col = 0;
}

void screen_set_color(uint8_t color) {
    current_color = color;
}

void set_cursor(uint8_t row, uint8_t col) {
    cursor_row = row;
    cursor_col = col;
}

void print_char(char c) {
    if (c == '\b') {
        if (cursor_col > 0) {
            cursor_col--;
        } else if (cursor_row > 0) {
            cursor_row--;
            cursor_col = VGA_COLS - 1;
        }
        vga_write(cursor_row, cursor_col, ' ', current_color);
        return;
    } else if (c == '\n') {
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

void print(const char* str) {
    for (int i = 0; str[i] != '\0'; i++) {
        print_char(str[i]);
    }
}

void print_color(const char* str, uint8_t color) {
    uint8_t saved = current_color;
    current_color = color;
    print(str);
    current_color = saved;
}

void print_line(const char* str) {
    print(str);
    print_char('\n');
}

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
