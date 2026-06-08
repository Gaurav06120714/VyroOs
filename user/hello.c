
#include "libvyro.h"

void _start() {
    vyro_println("  [hello] Second user binary running in ring 3.");
    vyro_print("  [hello] My PID: "); vyro_print_int(vyro_getpid()); vyro_putc('\n');
    vyro_print("  [hello] Up since boot: "); vyro_print_int(vyro_uptime_ms());
    vyro_println(" ms");
    vyro_println("  [hello] Press a number 1-3 from the shell to launch this again.");
    vyro_println("  [hello] Goodbye.");
    vyro_exit();
}
