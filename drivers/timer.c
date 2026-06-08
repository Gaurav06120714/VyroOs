#include "timer.h"
#include "pic.h"
#include "../kernel/isr.h"
#include "../kernel/sched.h"

static volatile uint64_t tick_count = 0;
static uint32_t          timer_freq = TIMER_HZ;

void timer_handler(registers_t* regs) {
    (void)regs;
    tick_count++;
    sched_tick();
}

void timer_init(uint32_t frequency) {
    if (frequency == 0) frequency = TIMER_HZ;
    timer_freq = frequency;


    uint32_t divisor = PIT_BASE_FREQ / frequency;
    if (divisor > 0xFFFF) divisor = 0xFFFF;
    if (divisor < 1)      divisor = 1;


    outb(PIT_COMMAND, 0x36);


    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));


    irq_register(0, timer_handler);
    pic_unmask_irq(0);
}

uint64_t timer_ticks() {
    return tick_count;
}

uint64_t timer_uptime_ms() {
    return (tick_count * 1000) / timer_freq;
}

uint64_t timer_uptime_seconds() {
    return tick_count / timer_freq;
}

static inline int irq_enabled(void) {
    uint64_t flags;
    __asm__ volatile("pushfq; popq %0" : "=r"(flags) :: "memory");
    return (flags >> 9) & 1;
}

void sleep_ms(uint32_t ms) {
    uint64_t target = tick_count + ((uint64_t)ms * timer_freq) / 1000;
    if (irq_enabled()) {
        while (tick_count < target)
            __asm__ volatile("hlt");
    } else {



        while (tick_count < target)
            __asm__ volatile("pause");
    }
}
