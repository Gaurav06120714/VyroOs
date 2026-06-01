#include "compositor.h"
#include "heap.h"
#include "../drivers/framebuffer.h"

// Back buffer: 1024 * 768 * 3 = 2.25 MB, allocated from kernel heap.
// We hold packed BGR pixels just like the framebuffer for fast blit.
static uint8_t* backbuf = 0;
static uint8_t* font    = (uint8_t*)0x80000;

#define BB_W  FB_WIDTH
#define BB_H  FB_HEIGHT
#define BB_PITCH (BB_W * 3)

// ─────────────────────────────────────────────────
// Initialize the back buffer
// ─────────────────────────────────────────────────
int comp_init() {
    if (backbuf) return 1;
    backbuf = (uint8_t*) kmalloc(BB_PITCH * BB_H);
    return backbuf ? 1 : 0;
}

uint32_t comp_width()  { return BB_W; }
uint32_t comp_height() { return BB_H; }

// ─────────────────────────────────────────────────
// Pixel ops on the back buffer
// ─────────────────────────────────────────────────
static inline void put(uint32_t x, uint32_t y, uint32_t color) {
    if (x >= BB_W || y >= BB_H || !backbuf) return;
    uint32_t off = y * BB_PITCH + x * 3;
    backbuf[off + 0] = (uint8_t)(color & 0xFF);
    backbuf[off + 1] = (uint8_t)((color >> 8) & 0xFF);
    backbuf[off + 2] = (uint8_t)((color >> 16) & 0xFF);
}

void comp_pixel(uint32_t x, uint32_t y, uint32_t color) { put(x, y, color); }

void comp_clear(uint32_t color) {
    if (!backbuf) return;
    uint8_t b = color & 0xFF, g = (color >> 8) & 0xFF, r = (color >> 16) & 0xFF;
    for (uint32_t y = 0; y < BB_H; y++) {
        uint8_t* row = backbuf + y * BB_PITCH;
        for (uint32_t x = 0; x < BB_W; x++) {
            row[x*3+0] = b; row[x*3+1] = g; row[x*3+2] = r;
        }
    }
}

void comp_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    for (uint32_t j = 0; j < h; j++)
        for (uint32_t i = 0; i < w; i++)
            put(x + i, y + j, color);
}

void comp_border(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    if (w == 0 || h == 0) return;
    for (uint32_t i = 0; i < w; i++) { put(x+i, y, color); put(x+i, y+h-1, color); }
    for (uint32_t j = 0; j < h; j++) { put(x, y+j, color); put(x+w-1, y+j, color); }
}

// Drop shadow: a darker rectangle offset down-right behind the window
void comp_shadow(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    for (uint32_t off = 1; off <= 5; off++) {
        // Fade alpha by darkening the color toward black
        uint8_t r = (color >> 16) & 0xFF, g = (color >> 8) & 0xFF, b = color & 0xFF;
        uint32_t fade = (6 - off);            // 5..1
        r = (uint8_t)(r * fade / 12);
        g = (uint8_t)(g * fade / 12);
        b = (uint8_t)(b * fade / 12);
        uint32_t c = (r << 16) | (g << 8) | b;
        // Right edge
        for (uint32_t j = 0; j < h; j++) put(x + w + off - 1, y + off + j, c);
        // Bottom edge
        for (uint32_t i = 0; i < w; i++) put(x + off + i, y + h + off - 1, c);
    }
}

void comp_glyph(uint32_t px, uint32_t py, char c, uint32_t fg, uint32_t bg) {
    uint8_t gi = (uint8_t)c;
    for (uint32_t gy = 0; gy < 16; gy++) {
        uint8_t row = font[gi * 16 + gy];
        for (uint32_t gx = 0; gx < 8; gx++)
            put(px + gx, py + gy, (row & (0x80 >> gx)) ? fg : bg);
    }
}

void comp_text(uint32_t px, uint32_t py, const char* s, uint32_t fg, uint32_t bg) {
    uint32_t x = px;
    for (int i = 0; s[i]; i++) { comp_glyph(x, py, s[i], fg, bg); x += 8; }
}

// Text where background = whatever's already there (no bg fill)
void comp_text_bg_alpha(uint32_t px, uint32_t py, const char* s, uint32_t fg) {
    uint32_t x = px;
    for (int i = 0; s[i]; i++) {
        uint8_t gi = (uint8_t)s[i];
        for (uint32_t gy = 0; gy < 16; gy++) {
            uint8_t row = font[gi * 16 + gy];
            for (uint32_t gx = 0; gx < 8; gx++)
                if (row & (0x80 >> gx)) put(x + gx, py + gy, fg);
        }
        x += 8;
    }
}

void comp_gradient_v(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                     uint32_t top, uint32_t bottom) {
    int tr = (top >> 16) & 0xFF, tg = (top >> 8) & 0xFF, tb = top & 0xFF;
    int br = (bottom >> 16) & 0xFF, bg = (bottom >> 8) & 0xFF, bb = bottom & 0xFF;
    for (uint32_t j = 0; j < h; j++) {
        // signed math so negative deltas work (e.g. bright→dark)
        int r = tr + (br - tr) * (int)j / (int)h;
        int g = tg + (bg - tg) * (int)j / (int)h;
        int b = tb + (bb - tb) * (int)j / (int)h;
        if (r < 0) r = 0; if (g < 0) g = 0; if (b < 0) b = 0;
        uint32_t c = ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
        for (uint32_t i = 0; i < w; i++) put(x + i, y + j, c);
    }
}

// ─────────────────────────────────────────────────
// comp_present: blit the back buffer to the actual framebuffer.
// One pass; the framebuffer is also BGR 24bpp so memcpy-equivalent.
// ─────────────────────────────────────────────────
extern uint8_t fb_available(void);
void comp_present() {
    if (!backbuf || !fb_available()) return;
    // We don't have direct fb pointer access — copy pixel by pixel.
    // This is the one place that matters for performance; for now,
    // straight-line copy is fine on QEMU.
    for (uint32_t y = 0; y < BB_H; y++) {
        for (uint32_t x = 0; x < BB_W; x++) {
            uint32_t off = y * BB_PITCH + x * 3;
            uint32_t color = ((uint32_t)backbuf[off+2] << 16) |
                             ((uint32_t)backbuf[off+1] << 8)  |
                              (uint32_t)backbuf[off+0];
            fb_putpixel(x, y, color);
        }
    }
}
