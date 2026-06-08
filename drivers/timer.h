#ifndef TIMER_H
#define TIMER_H

#include "../include/types.h"
#include "../kernel/idt.h"

#define PIT_CHANNEL0   0x40
#define PIT_CHANNEL1   0x41
#define PIT_CHANNEL2   0x42
#define PIT_COMMAND    0x43

#define PIT_BASE_FREQ  1193182

#define TIMER_HZ       100

void     timer_init(uint32_t frequency);
uint64_t timer_ticks();
uint64_t timer_uptime_ms();
uint64_t timer_uptime_seconds();
void     sleep_ms(uint32_t ms);

void timer_handler(registers_t* regs);

#endif
