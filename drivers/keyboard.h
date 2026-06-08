#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "../include/types.h"
#include "../kernel/idt.h"

#define KEYBOARD_DATA_PORT    0x60
#define KEYBOARD_STATUS_PORT  0x64

#define KEY_NONE       0x00
#define KEY_BACKSPACE  0x08
#define KEY_ENTER      '\n'
#define KEY_TAB        '\t'
#define KEY_ESCAPE     0x1B
#define KEY_SHIFT      0xF0
#define KEY_CAPS       0xF1

#define KB_BUFFER_SIZE 256

void keyboard_init();
char keyboard_getchar();
uint8_t keyboard_has_input();

void keyboard_handler(registers_t* regs);

#endif
