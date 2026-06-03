/* libvyro-linux — window object (B2 stub; B3 wires to compositor IPC) */
#include "../include/vyro.h"
#include <stdlib.h>
#include <string.h>

struct vyro_window {
    char title[128];
    int  w, h;
    uint32_t *pixels;
};

vyro_window_t *vyro_window_create(const char *title, int w, int h) {
    vyro_window_t *win = calloc(1, sizeof(*win));
    if (!win) return NULL;
    if (title) strncpy(win->title, title, sizeof(win->title) - 1);
    win->w = w; win->h = h;
    win->pixels = calloc((size_t)w * h, sizeof(uint32_t));
    if (!win->pixels) { free(win); return NULL; }
    return win;
}

void vyro_window_destroy(vyro_window_t *w) {
    if (!w) return;
    free(w->pixels);
    free(w);
}

void vyro_window_present(vyro_window_t *w) {
    (void)w; /* B3: IPC to vyro-compositor over a UNIX socket */
}
