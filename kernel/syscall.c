#include "syscall.h"
#include "idt.h"
#include "../drivers/screen.h"
#include "../drivers/timer.h"
#include "../include/types.h"

// Defined in isr_stubs.asm
extern void isr128();
// Defined in usermode.asm — unwinds back to the kernel from ring 3
extern void return_to_kernel();

// ─────────────────────────────────────────────────
// syscall_init: install the int 0x80 gate.
// DPL=3 (IDT_USER_GATE) so future ring-3 code can call it.
// ─────────────────────────────────────────────────
void syscall_init() {
    idt_set_gate(SYSCALL_VECTOR, (uint64_t)isr128, IDT_USER_GATE);
}

// ─────────────────────────────────────────────────
// syscall_dispatch: the kernel-side syscall handler.
// Reads the syscall number from rax, args from rdi/rsi/rdx,
// and writes the return value back into regs->rax.
// ─────────────────────────────────────────────────
void syscall_dispatch(registers_t* regs) {
    switch (regs->rax) {

        case SYS_WRITE: {
            const char* str = (const char*) regs->rdi;
            uint64_t count = 0;
            if (str) {
                while (str[count]) count++;
                print(str);
            }
            regs->rax = count;          // return bytes written
            break;
        }

        case SYS_GETPID:
            regs->rax = 1;              // kernel/shell pid
            break;

        case SYS_SLEEP:
            sleep_ms((uint32_t) regs->rdi);
            regs->rax = 0;
            break;

        case SYS_UPTIME:
            regs->rax = timer_uptime_ms();
            break;

        case SYS_CLEAR:
            screen_clear(WHITE_ON_BLACK);
            regs->rax = 0;
            break;

        case SYS_PUTCHAR:
            print_char((char) regs->rdi);
            regs->rax = 0;
            break;

        case SYS_VERSION:
            regs->rax = 25;             // current phase
            break;

        case SYS_TICKS:
            regs->rax = timer_ticks();
            break;

        case SYS_RAND: {
            // xorshift PRNG seeded by uptime
            static uint64_t seed = 0;
            if (!seed) seed = timer_ticks() | 1;
            seed ^= seed << 13;
            seed ^= seed >> 7;
            seed ^= seed << 17;
            regs->rax = seed;
            break;
        }

        case SYS_EXIT:
            // Ring-3 program is done — unwind back to the kernel.
            // Does not return.
            return_to_kernel();
            break;

        default:
            regs->rax = (uint64_t)-1;   // unknown syscall
            break;
    }
}
