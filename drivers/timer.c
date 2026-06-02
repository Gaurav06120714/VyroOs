#include "timer.h"
#include "pic.h"
#include "../kernel/isr.h"
#include "../kernel/sched.h"

// Global tick counter — incremented on every IRQ0
static volatile uint64_t tick_count = 0;
static uint32_t          timer_freq = TIMER_HZ;

// ─────────────────────────────────────────────────
// timer_handler: called on every PIT interrupt (IRQ0)
// Keep it minimal — runs in interrupt context
// ─────────────────────────────────────────────────
void timer_handler(registers_t* regs) {
    (void)regs;
    tick_count++;
    sched_tick();
}

// ─────────────────────────────────────────────────
// timer_init: program PIT channel 0 to fire at `frequency` Hz
// ─────────────────────────────────────────────────
void timer_init(uint32_t frequency) {
    if (frequency == 0) frequency = TIMER_HZ;
    timer_freq = frequency;

    // Calculate the divisor for the requested frequency
    uint32_t divisor = PIT_BASE_FREQ / frequency;
    if (divisor > 0xFFFF) divisor = 0xFFFF;   // 16-bit max
    if (divisor < 1)      divisor = 1;

    // Command byte: channel 0, access lo/hi byte, mode 3 (square wave), binary
    outb(PIT_COMMAND, 0x36);

    // Send divisor — low byte then high byte
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));

    // Register the IRQ0 handler and unmask it
    irq_register(0, timer_handler);
    pic_unmask_irq(0);
}

// ─────────────────────────────────────────────────
// timer_ticks: raw tick count since boot
// ─────────────────────────────────────────────────
uint64_t timer_ticks() {
    return tick_count;
}

// ─────────────────────────────────────────────────
// timer_uptime_ms: milliseconds since boot
// ─────────────────────────────────────────────────
uint64_t timer_uptime_ms() {
    return (tick_count * 1000) / timer_freq;
}

// ─────────────────────────────────────────────────
// timer_uptime_seconds: whole seconds since boot
// ─────────────────────────────────────────────────
uint64_t timer_uptime_seconds() {
    return tick_count / timer_freq;
}

// ─────────────────────────────────────────────────
// sleep_ms: busy-wait for `ms` milliseconds
// Halts the CPU between ticks to save power
// ─────────────────────────────────────────────────
void sleep_ms(uint32_t ms) {
    uint64_t target = tick_count + ((uint64_t)ms * timer_freq) / 1000;
    while (tick_count < target) {
        __asm__ volatile("hlt");
    }
}
