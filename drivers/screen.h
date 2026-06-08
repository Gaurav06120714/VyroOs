#ifndef SCREEN_H
#define SCREEN_H

#include "../include/types.h"

#define VGA_ADDRESS    0xb8000

#define VGA_COLS       128
#define VGA_ROWS       48

#define COLOR_BLACK        0x0
#define COLOR_BLUE         0x1
#define COLOR_GREEN        0x2
#define COLOR_CYAN         0x3
#define COLOR_RED          0x4
#define COLOR_MAGENTA      0x5
#define COLOR_BROWN        0x6
#define COLOR_LIGHT_GREY   0x7
#define COLOR_DARK_GREY    0x8
#define COLOR_LIGHT_BLUE   0x9
#define COLOR_LIGHT_GREEN  0xA
#define COLOR_LIGHT_CYAN   0xB
#define COLOR_LIGHT_RED    0xC
#define COLOR_PINK         0xD
#define COLOR_YELLOW       0xE
#define COLOR_WHITE        0xF

#define MAKE_COLOR(fg, bg) ((bg << 4) | fg)
#define WHITE_ON_BLACK     MAKE_COLOR(COLOR_WHITE, COLOR_BLACK)
#define WHITE_ON_BLUE      MAKE_COLOR(COLOR_WHITE, COLOR_BLUE)
#define YELLOW_ON_BLACK    MAKE_COLOR(COLOR_YELLOW, COLOR_BLACK)
#define CYAN_ON_BLACK      MAKE_COLOR(COLOR_LIGHT_CYAN, COLOR_BLACK)

void screen_init();
void screen_clear(uint8_t color);
void screen_set_color(uint8_t color);
void print_char(char c);
void print(const char* str);
void print_color(const char* str, uint8_t color);
void print_line(const char* str);
void print_int(uint64_t n);
void print_hex(uint64_t n);
void set_cursor(uint8_t row, uint8_t col);

#endif
