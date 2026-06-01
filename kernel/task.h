#ifndef TASK_H
#define TASK_H

#include "../include/types.h"

#define TASK_STACK_SIZE  8192    // 8KB stack per task
#define TASK_NAME_MAX    32
#define MAX_TASKS        16

// Task states
#define TASK_READY       0
#define TASK_RUNNING     1
#define TASK_FINISHED    2

typedef struct task {
    uint64_t      rsp;                  // Saved stack pointer (MUST be first field)
    uint64_t      id;
    char          name[TASK_NAME_MAX];
    uint8_t       state;
    struct task*  next;                 // Circular linked list
    uint8_t*      stack_base;           // Allocated stack (for freeing)
} task_t;

void   tasking_init();
task_t* task_create(const char* name, void (*entry)(void));
void   task_yield();
void   task_exit();
void   task_run_all();                  // Run scheduler until all tasks finish

uint32_t task_count();
task_t*  task_list_head();

#endif
