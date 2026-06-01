// ─────────────────────────────────────────────────
// Vyro OS sample application — built on the libvyro
// application framework (Phase 28). Compiled as a
// standalone ELF64, loaded by the kernel, run in ring 3.
// ─────────────────────────────────────────────────
#include "libvyro.h"

void _start() {
    vyro_print("  [app] Hello from a libvyro application!\n");
    vyro_print("  [app] Using the Vyro OS app framework.\n");

    long pid = vyro_getpid();
    (void)pid;

    vyro_print("  [app] Rolling dice via vyro_rand(): ");
    long r = vyro_rand() % 6 + 1;
    vyro_putc('0' + (char)r);
    vyro_putc('\n');

    vyro_print("  [app] Done. Exiting cleanly.\n");
    vyro_exit();

    for (;;) {}
}
