// ─────────────────────────────────────────────────
// Vyro OS — standalone user program (init)
// Compiled as a SEPARATE ELF64 binary, loaded by the
// kernel's ELF loader, and run in ring 3.
// Its only link to the kernel is int 0x80 (syscalls).
// ─────────────────────────────────────────────────

#define SYS_WRITE   1
#define SYS_GETPID  2
#define SYS_EXIT    8

static long syscall1(long num, long arg1) {
    long ret;
    __asm__ volatile("int $0x80"
                     : "=a"(ret)
                     : "a"(num), "D"(arg1)
                     : "memory");
    return ret;
}

// Entry point — the ELF e_entry will point here
void _start() {
    syscall1(SYS_WRITE, (long)"  [ELF] Loaded from a real ELF64 binary!\n");
    syscall1(SYS_WRITE, (long)"  [ELF] Parsed program headers, copied segments.\n");

    long pid = syscall1(SYS_GETPID, 0);
    (void)pid;
    syscall1(SYS_WRITE, (long)"  [ELF] Running unprivileged in ring 3.\n");

    syscall1(SYS_EXIT, 0);

    for (;;) {}   // never reached
}
