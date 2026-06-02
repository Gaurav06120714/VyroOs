#ifndef SCHED_H
#define SCHED_H

#include "../include/types.h"

// Timer-driven scheduler instrumentation.
// The existing task.c is cooperative — tasks call task_yield() manually.
// v3.12 adds a quantum tracker driven by the PIT IRQ: when a task has used
// its time slice, sched_should_yield() returns 1 and sched_check_preempt()
// performs the yield. Calling sched_check_preempt() from kernel hot paths
// (shell input loop, network pump, etc.) gives effective preemption at safe
// kernel boundaries without rebuilding the context switch path.

void sched_init(uint32_t quantum_ms);

// Called from the timer IRQ. Bumps the quantum counter.
void sched_tick(void);

// Returns 1 if the current task has used its quantum since the last yield.
int  sched_should_yield(void);

// If a yield is pending and a task is currently running, call task_yield().
void sched_check_preempt(void);

uint64_t sched_total_preempts(void);
uint64_t sched_total_ticks(void);

#endif
