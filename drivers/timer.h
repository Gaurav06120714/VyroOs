#ifndef TIMER_H
#define TIMER_H

#include "../include/types.h"
#include "../kernel/idt.h"

// PIT (8253/8254) I/O ports
#define PIT_CHANNEL0   0x40   // Channel 0 data — system timer
#define PIT_CHANNEL1   0x41   // Channel 1 — legacy DRAM refresh
#define PIT_CHANNEL2   0x42   // Channel 2 — PC speaker
#define PIT_COMMAND    0x43   // Mode/command register

// PIT base oscillator frequency (fixed in hardware)
#define PIT_BASE_FREQ  1193182

// Default timer frequency for Vyro OS (100 Hz = 10ms per tick)
#define TIMER_HZ       100

void     timer_init(uint32_t frequency);
uint64_t timer_ticks();
uint64_t timer_uptime_ms();
uint64_t timer_uptime_seconds();
void     sleep_ms(uint32_t ms);

// IRQ0 handler
void timer_handler(registers_t* regs);

#endif
