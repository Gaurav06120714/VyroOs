#ifndef COMPOSITOR_H
#define COMPOSITOR_H

#include "../include/types.h"

// ─────────────────────────────────────────────────
// Vyro OS Compositor v2 — double-buffered.
// All UI draws into a back buffer (RAM); comp_present()
// blits it to the visible framebuffer in one shot.
// Eliminates flicker, enables animation.
// ─────────────────────────────────────────────────

int  comp_init();                    // returns 1 on success
void comp_clear(uint32_t color);
void comp_pixel(uint32_t x, uint32_t y, uint32_t color);
void comp_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void comp_border(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void comp_shadow(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void comp_glyph(uint32_t px, uint32_t py, char c, uint32_t fg, uint32_t bg);
void comp_text(uint32_t px, uint32_t py, const char* s, uint32_t fg, uint32_t bg);
void comp_text_bg_alpha(uint32_t px, uint32_t py, const char* s, uint32_t fg);
void comp_gradient_v(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                     uint32_t top, uint32_t bottom);
void comp_present();                 // blit back buffer to framebuffer

uint32_t comp_width();
uint32_t comp_height();

#endif
