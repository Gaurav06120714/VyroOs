#include "task.h"
#include "heap.h"

extern void context_switch(uint64_t* old_rsp, uint64_t new_rsp);

static task_t* current      = 0;
static task_t* task_head    = 0;
static task_t  scheduler_ctx;
static uint32_t next_id     = 1;
static uint32_t num_tasks   = 0;

static void tstrcpy(char* dst, const char* src, int max) {
    int i = 0;
    while (src[i] && i < max - 1) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

void tasking_init() {
    current    = 0;
    task_head  = 0;
    next_id    = 1;
    num_tasks  = 0;
}

task_t* task_create(const char* name, void (*entry)(void)) {
    if (num_tasks >= MAX_TASKS) return 0;

    task_t* t = (task_t*) kmalloc_zero(sizeof(task_t));
    if (!t) return 0;

    t->stack_base = (uint8_t*) kmalloc(TASK_STACK_SIZE);
    if (!t->stack_base) { kfree(t); return 0; }

    t->id    = next_id++;
    t->state = TASK_READY;
    tstrcpy(t->name, name, TASK_NAME_MAX);


    uint64_t* sp = (uint64_t*)(t->stack_base + TASK_STACK_SIZE);


    *(--sp) = (uint64_t)entry;
    *(--sp) = 0;
    *(--sp) = 0;
    *(--sp) = 0;
    *(--sp) = 0;
    *(--sp) = 0;
    *(--sp) = 0;

    t->rsp = (uint64_t)sp;


    if (!task_head) {
        task_head = t;
        t->next   = t;
    } else {
        task_t* cur = task_head;
        while (cur->next != task_head) cur = cur->next;
        cur->next   = t;
        t->next     = task_head;
    }

    num_tasks++;
    return t;
}

static task_t* pick_next(task_t* from) {
    if (!from) return 0;
    task_t* t = from->next;
    for (uint32_t i = 0; i < MAX_TASKS + 1; i++) {
        if (t->state == TASK_READY || t->state == TASK_RUNNING) return t;
        t = t->next;
    }
    return 0;
}

void task_yield() {
    if (!current) return;

    task_t* next = pick_next(current);


    if (!next || next == current) {
        task_t* prev = current;
        if (prev->state == TASK_FINISHED) {
            current = 0;
            context_switch(&prev->rsp, scheduler_ctx.rsp);
        }
        return;
    }

    task_t* prev = current;
    current = next;
    next->state = TASK_RUNNING;
    if (prev->state == TASK_RUNNING) prev->state = TASK_READY;

    context_switch(&prev->rsp, next->rsp);
}

void task_exit() {
    if (!current) return;
    current->state = TASK_FINISHED;
    num_tasks--;

    task_t* next = pick_next(current);
    task_t* prev = current;

    if (!next || next == current) {

        current = 0;
        context_switch(&prev->rsp, scheduler_ctx.rsp);
    } else {
        current = next;
        next->state = TASK_RUNNING;
        context_switch(&prev->rsp, next->rsp);
    }
}

void task_run_all() {
    if (!task_head) return;

    current = task_head;
    current->state = TASK_RUNNING;



    context_switch(&scheduler_ctx.rsp, current->rsp);


    task_head = 0;
    num_tasks = 0;
    current   = 0;
}

uint32_t task_count()      { return num_tasks; }
task_t*  task_list_head()  { return task_head; }
