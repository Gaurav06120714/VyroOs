#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H
#include "../include/types.h"

#define FB_WIDTH    1024
#define FB_HEIGHT   768
#define FB_BPP      3        // 24bpp = 3 bytes per pixel
#define FB_PITCH    (FB_WIDTH * FB_BPP)
#define FONT_WIDTH  8
#define FONT_HEIGHT 16
#define FB_COLS     (FB_WIDTH / FONT_WIDTH)    // 128
#define FB_ROWS     (FB_HEIGHT / FONT_HEIGHT)  // 48

void fb_init(uint64_t lfb_addr);
uint8_t fb_available(void);
void fb_clear(uint32_t color);
void fb_putchar(uint32_t col, uint32_t row, char c, uint32_t fg, uint32_t bg);
void fb_putpixel(uint32_t x, uint32_t y, uint32_t color);

// color helpers — 24bpp RGB (stored as BGR in framebuffer)
#define FB_COLOR(r,g,b) (((uint32_t)(r)<<16)|((uint32_t)(g)<<8)|(b))
#define FB_BLACK   FB_COLOR(0,0,0)
#define FB_WHITE   FB_COLOR(255,255,255)
#define FB_GREEN   FB_COLOR(0,220,0)
#define FB_CYAN    FB_COLOR(0,220,220)
#define FB_YELLOW  FB_COLOR(220,220,0)
#define FB_RED     FB_COLOR(220,0,0)
#define FB_GREY    FB_COLOR(150,150,150)
#define FB_DGREY   FB_COLOR(60,60,60)
#define FB_BLUE    FB_COLOR(0,0,200)

#endif
