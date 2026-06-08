#ifndef SCHED_H
#define SCHED_H

#include "../include/types.h"

void sched_init(uint32_t quantum_ms);

void sched_tick(void);

int  sched_should_yield(void);

void sched_check_preempt(void);

void sched_request_yield(void);

uint64_t sched_total_preempts(void);
uint64_t sched_total_ticks(void);

uint32_t sched_quantum_ticks(void);
uint32_t sched_used_ticks(void);

#endif
