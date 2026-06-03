/* libvyro-linux — input poll (B5 wires to libinput / evdev) */
#include "../include/vyro.h"

int vyro_poll(vyro_window_t *w, vyro_event_t *out) {
    (void)w;
    if (out) out->kind = VYRO_EV_NONE;
    return 0;
}
