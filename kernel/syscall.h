#ifndef SYSCALL_H
#define SYSCALL_H

#include "idt.h"

#define SYS_WRITE    1
#define SYS_GETPID   2
#define SYS_SLEEP    3
#define SYS_UPTIME   4
#define SYS_CLEAR    5
#define SYS_PUTCHAR  6
#define SYS_VERSION  7
#define SYS_EXIT     8
#define SYS_TICKS    9
#define SYS_RAND    10

#define SYSCALL_VECTOR 0x80

void syscall_init();

void syscall_dispatch(registers_t* regs);

#endif
