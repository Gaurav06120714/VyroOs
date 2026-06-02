#include "sched.h"
#include "task.h"
#include "../drivers/timer.h"

static volatile uint64_t total_ticks   = 0;
static volatile uint64_t total_preempts = 0;
static volatile uint32_t quantum_ticks = 0;     // ticks per quantum
static volatile uint32_t used_ticks    = 0;     // ticks used by current task

void sched_init(uint32_t quantum_ms) {
    // Assume PIT runs at TIMER_HZ.
    uint32_t hz = TIMER_HZ;
    quantum_ticks = (quantum_ms * hz) / 1000;
    if (quantum_ticks < 1) quantum_ticks = 1;
    used_ticks = 0;
}

void sched_tick(void) {
    total_ticks++;
    used_ticks++;
}

int sched_should_yield(void) {
    return used_ticks >= quantum_ticks;
}

void sched_check_preempt(void) {
    if (used_ticks < quantum_ticks) return;
    used_ticks = 0;
    // Only yield if there is more than one runnable task.
    task_t* head = task_list_head();
    if (head && head->next && head->next != head) {
        total_preempts++;
        task_yield();
    }
}

uint64_t sched_total_preempts(void) { return total_preempts; }
uint64_t sched_total_ticks(void)    { return total_ticks; }
