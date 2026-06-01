#ifndef SYSCALL_H
#define SYSCALL_H

#include "idt.h"

// ─────────────────────────────────────────────────
// System call numbers (the Vyro OS ABI)
// ─────────────────────────────────────────────────
#define SYS_WRITE    1   // rdi = string pointer        → bytes written
#define SYS_GETPID   2   //                              → current pid
#define SYS_SLEEP    3   // rdi = milliseconds           → 0
#define SYS_UPTIME   4   //                              → uptime in ms
#define SYS_CLEAR    5   //                              → 0
#define SYS_PUTCHAR  6   // rdi = character              → 0
#define SYS_VERSION  7   //                              → version code (e.g. 13)
#define SYS_EXIT     8   // ring-3 program returns to kernel
#define SYS_TICKS    9   // → timer ticks since boot
#define SYS_RAND    10   // → pseudo-random 32-bit number

#define SYSCALL_VECTOR 0x80

void syscall_init();

// Called by the int 0x80 stub
void syscall_dispatch(registers_t* regs);

#endif
