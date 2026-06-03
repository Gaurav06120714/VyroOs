// ─────────────────────────────────────────────────
// libvyro — the Vyro OS application framework
// User programs include this header and call these
// functions, which wrap the kernel syscall ABI (int 0x80).
// This is the stable interface between apps and the OS.
// ─────────────────────────────────────────────────
#ifndef LIBVYRO_H
#define LIBVYRO_H

// Syscall numbers (must match kernel/syscall.h)
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

static inline long _syscall(long n, long a) {
    long r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(n), "D"(a) : "memory");
    return r;
}

// ── Public application API ──
static inline void  vyro_print(const char* s)  { _syscall(SYS_WRITE, (long)s); }
static inline void  vyro_putc(char c)          { _syscall(SYS_PUTCHAR, c); }
static inline void  vyro_clear(void)           { _syscall(SYS_CLEAR, 0); }
static inline long  vyro_getpid(void)          { return _syscall(SYS_GETPID, 0); }
static inline long  vyro_uptime_ms(void)       { return _syscall(SYS_UPTIME, 0); }
static inline long  vyro_ticks(void)           { return _syscall(SYS_TICKS, 0); }
static inline long  vyro_rand(void)            { return _syscall(SYS_RAND, 0); }
static inline void  vyro_sleep(long ms)        { _syscall(SYS_SLEEP, ms); }
static inline void  vyro_exit(void)            { _syscall(SYS_EXIT, 0); }

// ── Userspace utilities (no syscalls; pure user code) ──

static inline unsigned long vyro_strlen(const char* s) {
    unsigned long n = 0;
    while (s && s[n]) n++;
    return n;
}

static inline int vyro_strcmp(const char* a, const char* b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

static inline void vyro_print_int(long v) {
    if (v < 0) { vyro_putc('-'); v = -v; }
    char buf[32]; int n = 0;
    if (v == 0) { vyro_putc('0'); return; }
    while (v > 0) { buf[n++] = (char)('0' + (v % 10)); v /= 10; }
    while (n-- > 0) vyro_putc(buf[n]);
}

static inline void vyro_print_hex(unsigned long v) {
    vyro_putc('0'); vyro_putc('x');
    int started = 0;
    for (int i = 60; i >= 0; i -= 4) {
        int nib = (int)((v >> i) & 0xF);
        if (nib || started || i == 0) {
            vyro_putc(nib < 10 ? (char)('0' + nib) : (char)('A' + nib - 10));
            started = 1;
        }
    }
}

static inline void vyro_println(const char* s) { vyro_print(s); vyro_putc('\n'); }

#endif
