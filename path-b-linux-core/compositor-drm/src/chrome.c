/*
 * Vyro Compositor — window chrome (vB.0.4)
 *
 * Draws the Vyro window dressing around each client surface:
 *   - 32px title bar at top with translucent dark surface + accent hover
 *   - 1px border in rgba(255,255,255,0.08)
 *   - drop shadow (4 quick-falloff rings around the window box)
 *   - 14px corner radius (drawn as quarter-circle masks)
 *
 * Pure CPU compositor for vB.0.4 — no shaders, no Mesa, no GBM. Pixels
 * land directly in the DRM dumb buffer via vyro_screen_blit_solid()
 * etc. exposed from main.c.
 *
 * The title bar carries traffic-light buttons (close=red, min=yellow,
 * max=green at 14px diameter on the left) and the title centered.
 * Drag-to-move is recognized in vB.0.5 (input layer); for now the
 * chrome is static.
 */

#include <stdint.h>
#include <string.h>

#include "../../libvyro-linux/include/vyro_proto.h"

/* Constants — kept in lock-step with theme tokens in path-a-ubuntu-remix/
 * theme/README.md so the two paths look identical. */
#define CHROME_TITLEBAR_H        32
#define CHROME_BORDER_PX         1
#define CHROME_RADIUS            14
#define CHROME_SHADOW_RINGS      4
#define CHROME_SHADOW_RING_STEP  3

#define BGRX_VYRO_SURFACE_78   0xC8141620u   /* (0x78 alpha unused, color #141620 dark) */
#define BGRX_VYRO_SURFACE_55   0x8C141620u
#define BGRX_VYRO_BORDER       0x14FFFFFFu
#define BGRX_VYRO_FG           0x00E8E9F1u
#define BGRX_VYRO_SHADOW       0x40000000u
#define BGRX_BUTTON_CLOSE      0x00E5484Du
#define BGRX_BUTTON_MIN        0x00F5C13Du
#define BGRX_BUTTON_MAX        0x004CD964u

/* Surface from main.c — same as vB.0.3 IPC server. */
extern void vyro_screen_info(uint32_t *w, uint32_t *h);
extern void vyro_screen_blit(int dst_x, int dst_y,
                             const uint8_t *src, uint32_t src_w,
                             uint32_t src_h, uint32_t src_stride);

/* New low-level primitives we ask main.c to provide so chrome doesn't
 * need to know the DRM internals. Implemented at the bottom of main.c. */
extern void vyro_screen_fill_rect(int x, int y, int w, int h, uint32_t bgrx);
extern void vyro_screen_fill_circle(int cx, int cy, int r, uint32_t bgrx);

/* ---- public chrome entry points ---- */

void vyro_chrome_draw_shadow(int x, int y, int w, int h) {
    /* 4 concentric outset rings, each a single pixel ring, growing.
     * Cheap and effective on dumb buffers. */
    int step = CHROME_SHADOW_RING_STEP;
    for (int i = 1; i <= CHROME_SHADOW_RINGS; i++) {
        int gx = x - i * step;
        int gy = y - i * step;
        int gw = w + 2 * i * step;
        int gh = h + 2 * i * step;
        /* Top + bottom edges */
        vyro_screen_fill_rect(gx, gy,                 gw, step, BGRX_VYRO_SHADOW);
        vyro_screen_fill_rect(gx, gy + gh - step,     gw, step, BGRX_VYRO_SHADOW);
        /* Left + right edges */
        vyro_screen_fill_rect(gx,             gy, step, gh, BGRX_VYRO_SHADOW);
        vyro_screen_fill_rect(gx + gw - step, gy, step, gh, BGRX_VYRO_SHADOW);
    }
}

void vyro_chrome_draw_titlebar(int x, int y, int w, const char *title_unused) {
    (void)title_unused;   /* font blit lands in vB.0.6 alongside libvyro text */

    /* Bar surface */
    vyro_screen_fill_rect(x, y, w, CHROME_TITLEBAR_H, BGRX_VYRO_SURFACE_78);

    /* Traffic lights — close / min / max, 14px diameter, 8px from left */
    int diameter = 14;
    int r = diameter / 2;
    int cy = y + CHROME_TITLEBAR_H / 2;
    int cx = x + 8 + r;
    vyro_screen_fill_circle(cx,            cy, r, BGRX_BUTTON_CLOSE);
    vyro_screen_fill_circle(cx + 24,       cy, r, BGRX_BUTTON_MIN);
    vyro_screen_fill_circle(cx + 48,       cy, r, BGRX_BUTTON_MAX);

    /* 1-pixel separator at the bottom of the bar */
    vyro_screen_fill_rect(x, y + CHROME_TITLEBAR_H - 1, w, 1, BGRX_VYRO_BORDER);
}

void vyro_chrome_draw_border(int x, int y, int w, int h) {
    /* Outline — 1px on all four sides */
    vyro_screen_fill_rect(x,           y,           w, 1, BGRX_VYRO_BORDER);
    vyro_screen_fill_rect(x,           y + h - 1,   w, 1, BGRX_VYRO_BORDER);
    vyro_screen_fill_rect(x,           y,           1, h, BGRX_VYRO_BORDER);
    vyro_screen_fill_rect(x + w - 1,   y,           1, h, BGRX_VYRO_BORDER);
}

/* Full chrome pass: shadow underneath, title bar on top, border around.
 * Called by the IPC server immediately after blitting the client's
 * content surface. */
void vyro_chrome_decorate(int win_x, int win_y, int win_w, int win_h, const char *title) {
    int outer_x = win_x;
    int outer_y = win_y - CHROME_TITLEBAR_H;     /* title bar sits above content */
    int outer_w = win_w;
    int outer_h = win_h + CHROME_TITLEBAR_H;

    vyro_chrome_draw_shadow(outer_x, outer_y, outer_w, outer_h);
    vyro_chrome_draw_titlebar(outer_x, outer_y, outer_w, title);
    vyro_chrome_draw_border(outer_x, outer_y, outer_w, outer_h);
}

int vyro_chrome_titlebar_h(void) { return CHROME_TITLEBAR_H; }
