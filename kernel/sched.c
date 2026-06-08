#include "sched.h"
#include "task.h"
#include "../drivers/timer.h"

static volatile uint64_t total_ticks   = 0;
static volatile uint64_t total_preempts = 0;
static volatile uint32_t quantum_ticks = 0;
static volatile uint32_t used_ticks    = 0;

void sched_init(uint32_t quantum_ms) {

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

    task_t* head = task_list_head();
    if (head && head->next && head->next != head) {
        total_preempts++;
        task_yield();
    }
}

void sched_request_yield(void) { used_ticks = quantum_ticks; }

uint64_t sched_total_preempts(void) { return total_preempts; }
uint64_t sched_total_ticks(void)    { return total_ticks; }
uint32_t sched_quantum_ticks(void)  { return quantum_ticks; }
uint32_t sched_used_ticks(void)     { return used_ticks; }
