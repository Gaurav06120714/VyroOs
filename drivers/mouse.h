#ifndef MOUSE_H
#define MOUSE_H

#include "../include/types.h"
#include "../kernel/idt.h"

#define MOUSE_LEFT   0x01
#define MOUSE_RIGHT  0x02
#define MOUSE_MIDDLE 0x04

#define MOUSE_BTN_LEFT   1
#define MOUSE_BTN_RIGHT  2

void   mouse_init();
int    mouse_x();
int    mouse_y();
uint8_t mouse_buttons();
void   mouse_handler(registers_t* regs);

#endif
