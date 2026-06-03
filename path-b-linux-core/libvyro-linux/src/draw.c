/* libvyro-linux — drawing primitives */
#include "../include/vyro.h"
#include <stdint.h>

struct vyro_window {
    char title[128];
    int  w, h;
    uint32_t *pixels;
};

void vyro_fill(vyro_window_t *win, uint32_t bgrx) {
    if (!win || !win->pixels) return;
    size_t n = (size_t)win->w * win->h;
    for (size_t i = 0; i < n; i++) win->pixels[i] = bgrx;
}

void vyro_rect(vyro_window_t *win, int x, int y, int cx, int cy, uint32_t bgrx) {
    if (!win || !win->pixels) return;
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = (x + cx) > win->w ? win->w : (x + cx);
    int y1 = (y + cy) > win->h ? win->h : (y + cy);
    for (int yy = y0; yy < y1; yy++)
        for (int xx = x0; xx < x1; xx++)
            win->pixels[yy * win->w + xx] = bgrx;
}

void vyro_text(vyro_window_t *win, int x, int y, const char *s, uint32_t bgrx) {
    /* B6: real font glyph blit. Stub: 1px-per-char advance marker. */
    if (!win || !s) return;
    int cx = x;
    while (*s) {
        vyro_rect(win, cx, y, 6, 10, bgrx);
        cx += 8;
        s++;
    }
}
