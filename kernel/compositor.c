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
    // vC.6.12: always re-allocate if backbuf isn't currently sane.
    // The static "if (backbuf) return 1" short-circuit was preserving a
    // corrupted-to-0xFFFFFFFFFFFFFFFF pointer from a previous run /
    // earlier subsystem; checking sanity instead means a fresh kmalloc
    // happens whenever the existing pointer is bad.
    if ((uint64_t)backbuf >= 0x500000ULL && (uint64_t)backbuf < 0xD00000ULL) return 1;
    backbuf = (uint8_t*) kmalloc(BB_PITCH * BB_H);
    return backbuf ? 1 : 0;
}

// vC.6.12: callable from the render loop to recover from in-flight
// corruption of the backbuf global by some OOB writer elsewhere in BSS.
void comp_revalidate(void) {
    if ((uint64_t)backbuf < 0x500000ULL || (uint64_t)backbuf >= 0xD00000ULL) {
        backbuf = (uint8_t*) kmalloc(BB_PITCH * BB_H);
    }
}

uint32_t comp_width()  { return BB_W; }
uint32_t comp_height() { return BB_H; }

// ─────────────────────────────────────────────────
// Pixel ops on the back buffer
// ─────────────────────────────────────────────────
// vC.6.11.1: validate backbuf is in the kernel heap range. CR2 capture
// in vC.6.11.0 revealed that something was corrupting the backbuf
// pointer to 0xFFFFFFFFFFFFFFFF — same value an int -1 sign-extends to
// when read as a pointer. The corruption source is still unknown
// (probably an out-of-bounds write nearby in BSS), but rejecting any
// backbuf outside the heap range [HEAP_START, HEAP_START+HEAP_SIZE]
// stops the kernel taking a page fault and lets the GUI render-loop
// keep going (it just paints nothing that frame, which is acceptable).
static inline int backbuf_sane(void) {
    uint64_t p = (uint64_t)backbuf;
    return p >= 0x500000ULL && p < 0xD00000ULL;
}

static inline void put(uint32_t x, uint32_t y, uint32_t color) {
    if (x >= BB_W || y >= BB_H) return;
    if (!backbuf_sane()) return;
    uint32_t off = y * BB_PITCH + x * 3;
    if (off + 2 >= (uint32_t)(BB_PITCH * BB_H)) return;
    backbuf[off + 0] = (uint8_t)(color & 0xFF);
    backbuf[off + 1] = (uint8_t)((color >> 8) & 0xFF);
    backbuf[off + 2] = (uint8_t)((color >> 16) & 0xFF);
}

void comp_pixel(uint32_t x, uint32_t y, uint32_t color) { put(x, y, color); }

void comp_clear(uint32_t color) {
    if (!backbuf_sane()) return;
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
// Glassmorphism primitives (v3.15)
// ─────────────────────────────────────────────────

static inline void get_bgr(uint32_t x, uint32_t y, uint8_t out[3]) {
    if (x >= BB_W || y >= BB_H || !backbuf_sane()) { out[0]=out[1]=out[2]=0; return; }
    uint32_t off = y * BB_PITCH + x * 3;
    out[0] = backbuf[off + 0];
    out[1] = backbuf[off + 1];
    out[2] = backbuf[off + 2];
}
static inline void put_bgr(uint32_t x, uint32_t y, uint8_t b, uint8_t g, uint8_t r) {
    if (x >= BB_W || y >= BB_H || !backbuf_sane()) return;
    uint32_t off = y * BB_PITCH + x * 3;
    backbuf[off + 0] = b;
    backbuf[off + 1] = g;
    backbuf[off + 2] = r;
}

void comp_blur_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint8_t passes) {
    if (!backbuf || passes == 0) return;
    if (x >= BB_W || y >= BB_H) return;
    if (x + w > BB_W) w = BB_W - x;
    if (y + h > BB_H) h = BB_H - y;

    for (uint8_t pass = 0; pass < passes; pass++) {
        // 3-tap horizontal then vertical box blur.
        for (uint32_t yy = y; yy < y + h; yy++) {
            for (uint32_t xx = x + 1; xx + 1 < x + w; xx++) {
                uint8_t a[3], b[3], c[3];
                get_bgr(xx - 1, yy, a);
                get_bgr(xx,     yy, b);
                get_bgr(xx + 1, yy, c);
                put_bgr(xx, yy,
                        (a[0] + b[0] + c[0]) / 3,
                        (a[1] + b[1] + c[1]) / 3,
                        (a[2] + b[2] + c[2]) / 3);
            }
        }
        for (uint32_t xx = x; xx < x + w; xx++) {
            for (uint32_t yy = y + 1; yy + 1 < y + h; yy++) {
                uint8_t a[3], b[3], c[3];
                get_bgr(xx, yy - 1, a);
                get_bgr(xx, yy,     b);
                get_bgr(xx, yy + 1, c);
                put_bgr(xx, yy,
                        (a[0] + b[0] + c[0]) / 3,
                        (a[1] + b[1] + c[1]) / 3,
                        (a[2] + b[2] + c[2]) / 3);
            }
        }
    }
}

void comp_tint_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                    uint32_t tint, uint8_t opacity) {
    if (!backbuf) return;
    if (x + w > BB_W) w = BB_W - x;
    if (y + h > BB_H) h = BB_H - y;
    uint8_t tb = tint & 0xFF, tg = (tint >> 8) & 0xFF, tr = (tint >> 16) & 0xFF;
    uint32_t inv = 255 - opacity;
    for (uint32_t yy = y; yy < y + h; yy++) {
        for (uint32_t xx = x; xx < x + w; xx++) {
            uint8_t cur[3];
            get_bgr(xx, yy, cur);
            put_bgr(xx, yy,
                    (uint8_t)((cur[0] * inv + tb * opacity) / 255),
                    (uint8_t)((cur[1] * inv + tg * opacity) / 255),
                    (uint8_t)((cur[2] * inv + tr * opacity) / 255));
        }
    }
}

static int inside_rounded(int32_t lx, int32_t ly, uint32_t w, uint32_t h, uint32_t r) {
    if (r == 0) return 1;
    int32_t W = (int32_t)w, H = (int32_t)h, R = (int32_t)r;
    int32_t cx, cy;
    if (lx < R && ly < R)             { cx = R - 1;     cy = R - 1; }
    else if (lx >= W - R && ly < R)   { cx = W - R;     cy = R - 1; }
    else if (lx < R && ly >= H - R)   { cx = R - 1;     cy = H - R; }
    else if (lx >= W - R && ly >= H - R) { cx = W - R; cy = H - R; }
    else return 1;
    int32_t dx = lx - cx, dy = ly - cy;
    return (dx * dx + dy * dy) < (R * R);
}

void comp_rounded_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                       uint32_t radius, uint32_t color) {
    uint8_t b = color & 0xFF, g = (color >> 8) & 0xFF, r = (color >> 16) & 0xFF;
    for (uint32_t yy = 0; yy < h; yy++) {
        for (uint32_t xx = 0; xx < w; xx++) {
            if (inside_rounded((int32_t)xx, (int32_t)yy, w, h, radius)) {
                put_bgr(x + xx, y + yy, b, g, r);
            }
        }
    }
}

void comp_glass_panel(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                      uint32_t tint, uint8_t opacity, uint32_t border_radius,
                      uint32_t border_color) {
    // 1. Blur the area underneath
    comp_blur_rect(x, y, w, h, 2);
    // 2. Tint with a semi-transparent color over the blur
    uint8_t tb = tint & 0xFF, tg = (tint >> 8) & 0xFF, tr = (tint >> 16) & 0xFF;
    uint32_t inv = 255 - opacity;
    for (uint32_t yy = 0; yy < h; yy++) {
        for (uint32_t xx = 0; xx < w; xx++) {
            if (!inside_rounded((int32_t)xx, (int32_t)yy, w, h, border_radius)) continue;
            uint8_t cur[3];
            get_bgr(x + xx, y + yy, cur);
            put_bgr(x + xx, y + yy,
                    (uint8_t)((cur[0] * inv + tb * opacity) / 255),
                    (uint8_t)((cur[1] * inv + tg * opacity) / 255),
                    (uint8_t)((cur[2] * inv + tr * opacity) / 255));
        }
    }
    // 3. Subtle border highlight along the rounded edge
    uint8_t bb = border_color & 0xFF;
    uint8_t bg = (border_color >> 8) & 0xFF;
    uint8_t br = (border_color >> 16) & 0xFF;
    for (uint32_t yy = 0; yy < h; yy++) {
        for (uint32_t xx = 0; xx < w; xx++) {
            int in_curr = inside_rounded((int32_t)xx, (int32_t)yy, w, h, border_radius);
            int in_inner = inside_rounded((int32_t)xx - 1, (int32_t)yy - 1, w - 2, h - 2,
                                          border_radius > 1 ? border_radius - 1 : 0);
            if (in_curr && !in_inner) put_bgr(x + xx, y + yy, bb, bg, br);
        }
    }
}

// ─────────────────────────────────────────────────
// comp_present: blit the back buffer to the actual framebuffer.
// One pass; the framebuffer is also BGR 24bpp so memcpy-equivalent.
// ─────────────────────────────────────────────────
extern uint8_t fb_available(void);
void comp_present() {
    if (!backbuf_sane() || !fb_available()) return;
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
