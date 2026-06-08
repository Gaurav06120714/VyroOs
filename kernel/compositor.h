#ifndef COMPOSITOR_H
#define COMPOSITOR_H

#include "../include/types.h"

int  comp_init();
void comp_revalidate(void);
int  comp_canary_ok(void);
void comp_canary_dump(void);
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
void comp_present();

uint32_t comp_width();
uint32_t comp_height();

void comp_blur_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint8_t passes);
void comp_tint_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                    uint32_t tint, uint8_t opacity);
void comp_glass_panel(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                      uint32_t tint, uint8_t opacity, uint32_t border_radius,
                      uint32_t border_color);
void comp_rounded_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                       uint32_t radius, uint32_t color);

#endif
