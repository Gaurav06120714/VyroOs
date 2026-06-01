#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "../include/types.h"
#include "../kernel/idt.h"

// PS/2 keyboard ports
#define KEYBOARD_DATA_PORT    0x60   // Read scan codes here
#define KEYBOARD_STATUS_PORT  0x64   // Status register

// Special key codes (returned by keyboard_getchar)
#define KEY_NONE       0x00
#define KEY_BACKSPACE  0x08
#define KEY_ENTER      '\n'
#define KEY_TAB        '\t'
#define KEY_ESCAPE     0x1B
#define KEY_SHIFT      0xF0   // Internal: shift state
#define KEY_CAPS       0xF1   // Internal: caps lock

// Input buffer size
#define KB_BUFFER_SIZE 256

void keyboard_init();
char keyboard_getchar();
uint8_t keyboard_has_input();

// Called by IRQ1 handler
void keyboard_handler(registers_t* regs);

#endif
