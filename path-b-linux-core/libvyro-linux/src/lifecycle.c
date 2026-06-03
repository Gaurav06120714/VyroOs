/* libvyro-linux — lifecycle (init / shutdown) */
#include "../include/vyro.h"
#include <stdio.h>

static int g_initialized = 0;

int vyro_init(void) {
    if (g_initialized) return 0;
    g_initialized = 1;
    return 0;
}

void vyro_shutdown(void) {
    g_initialized = 0;
}
