#include "framebuffer.h"

// Framebuffer pointer (set from LFB address passed to fb_init)
static uint8_t* fb     = 0;
// BIOS 8x16 font — copied to physical 0x80000 by bootloader
static uint8_t* font   = (uint8_t*)0x80000;
// Whether framebuffer is initialised and usable
static uint8_t  fb_ready = 0;

// ─────────────────────────────────────────────────
// fb_init: set the framebuffer base address
// ─────────────────────────────────────────────────
void fb_init(uint64_t lfb_addr) {
    if (lfb_addr == 0) {
        fb_ready = 0;
        return;
    }
    fb       = (uint8_t*)lfb_addr;
    fb_ready = 1;
}

// ─────────────────────────────────────────────────
// fb_available: returns 1 if fb is initialised
// ─────────────────────────────────────────────────
uint8_t fb_available(void) {
    return fb_ready;
}

// ─────────────────────────────────────────────────
// fb_putpixel: draw a single pixel at (x, y)
// color is 0x00RRGGBB; stored as BGR in memory
// ─────────────────────────────────────────────────
void fb_putpixel(uint32_t x, uint32_t y, uint32_t color) {
    if (x >= FB_WIDTH || y >= FB_HEIGHT) return;
    uint32_t offset = y * FB_PITCH + x * FB_BPP;
    fb[offset + 0] = (uint8_t)(color & 0xFF);         // Blue
    fb[offset + 1] = (uint8_t)((color >> 8) & 0xFF);  // Green
    fb[offset + 2] = (uint8_t)((color >> 16) & 0xFF); // Red
}

// ─────────────────────────────────────────────────
// fb_putchar: draw a character glyph at text cell (col, row)
// Uses BIOS 8x16 font from physical 0x80000
// ─────────────────────────────────────────────────
void fb_putchar(uint32_t col, uint32_t row, char c, uint32_t fg, uint32_t bg) {
    uint32_t px = col * FONT_WIDTH;
    uint32_t py = row * FONT_HEIGHT;
    uint8_t  glyph_index = (uint8_t)c;

    for (uint32_t gy = 0; gy < FONT_HEIGHT; gy++) {
        uint8_t glyph_row = font[glyph_index * 16 + gy];
        for (uint32_t gx = 0; gx < FONT_WIDTH; gx++) {
            // Bit 7 is the leftmost pixel
            uint32_t color = (glyph_row & (0x80 >> gx)) ? fg : bg;
            fb_putpixel(px + gx, py + gy, color);
        }
    }
}

// ─────────────────────────────────────────────────
// fb_clear: fill entire framebuffer with a solid color
// ─────────────────────────────────────────────────
void fb_clear(uint32_t color) {
    uint8_t b = (uint8_t)(color & 0xFF);
    uint8_t g = (uint8_t)((color >> 8) & 0xFF);
    uint8_t r = (uint8_t)((color >> 16) & 0xFF);

    for (uint32_t y = 0; y < FB_HEIGHT; y++) {
        for (uint32_t x = 0; x < FB_WIDTH; x++) {
            uint32_t offset = y * FB_PITCH + x * FB_BPP;
            fb[offset + 0] = b;
            fb[offset + 1] = g;
            fb[offset + 2] = r;
        }
    }
}

// ─────────────────────────────────────────────────
// fb_fill_rect: filled rectangle
// ─────────────────────────────────────────────────
void fb_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    for (uint32_t j = 0; j < h; j++)
        for (uint32_t i = 0; i < w; i++)
            fb_putpixel(x + i, y + j, color);
}

// ─────────────────────────────────────────────────
// fb_draw_rect: 1px rectangle outline
// ─────────────────────────────────────────────────
void fb_draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    for (uint32_t i = 0; i < w; i++) { fb_putpixel(x+i, y, color); fb_putpixel(x+i, y+h-1, color); }
    for (uint32_t j = 0; j < h; j++) { fb_putpixel(x, y+j, color); fb_putpixel(x+w-1, y+j, color); }
}

// ─────────────────────────────────────────────────
// fb_draw_glyph: draw one char at pixel position
// ─────────────────────────────────────────────────
void fb_draw_glyph(uint32_t px, uint32_t py, char c, uint32_t fg, uint32_t bg) {
    uint8_t gi = (uint8_t)c;
    for (uint32_t gy = 0; gy < FONT_HEIGHT; gy++) {
        uint8_t row = font[gi * 16 + gy];
        for (uint32_t gx = 0; gx < FONT_WIDTH; gx++) {
            fb_putpixel(px + gx, py + gy, (row & (0x80 >> gx)) ? fg : bg);
        }
    }
}

// ─────────────────────────────────────────────────
// fb_draw_text: draw a string at pixel position
// ─────────────────────────────────────────────────
void fb_draw_text(uint32_t px, uint32_t py, const char* s, uint32_t fg, uint32_t bg) {
    uint32_t x = px;
    for (int i = 0; s[i]; i++) {
        fb_draw_glyph(x, py, s[i], fg, bg);
        x += FONT_WIDTH;
    }
}

// ─────────────────────────────────────────────────
// fb_get_pixel: read a pixel (for cursor save-under)
// ─────────────────────────────────────────────────
uint32_t fb_get_pixel(uint32_t x, uint32_t y) {
    if (x >= FB_WIDTH || y >= FB_HEIGHT) return 0;
    uint8_t* p = fb + y * FB_PITCH + x * FB_BPP;
    return ((uint32_t)p[2] << 16) | ((uint32_t)p[1] << 8) | p[0];
}
