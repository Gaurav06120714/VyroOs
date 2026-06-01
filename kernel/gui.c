#include "gui.h"
#include "compositor.h"
#include "theme.h"
#include "../drivers/framebuffer.h"
#include "../drivers/mouse.h"
#include "../drivers/keyboard.h"
#include "../drivers/timer.h"
#include "../drivers/rtc.h"
#include "../include/types.h"

#define TOPBAR_H   28
#define DOCK_H     60
#define TITLE_H    26
#define MAX_WINS   4
#define DOCK_ITEMS 6

typedef struct {
    int x, y, w, h;
    const char* title;
    const char* body;
    uint8_t visible;
} window_t;

static window_t wins[MAX_WINS];
static int      win_count = 0;

// Dock items (name shown under, single-char icon shown on top)
static const char* dock_names[DOCK_ITEMS] = { "Finder","Term","Settings","Browser","Apps","Trash" };
static const char  dock_icons[DOCK_ITEMS] = { 'F','>','S','W','A','T' };

// ─── 2-digit helper for clock ───
static void d2(char* buf, int n) { buf[0] = '0' + (n/10); buf[1] = '0' + (n%10); }

// ─── Draw a window with shadow, title bar, body ───
static void draw_window(window_t* w, int focused) {
    const theme_t* t = theme();
    comp_shadow(w->x, w->y, w->w, w->h, t->win_shadow);
    uint32_t tcol = focused ? t->win_title_focus : t->win_title;
    comp_rect(w->x, w->y, w->w, TITLE_H, tcol);
    comp_rect(w->x, w->y + TITLE_H, w->w, w->h - TITLE_H, t->win_body);
    comp_border(w->x, w->y, w->w, w->h, t->win_border);

    // Traffic-light buttons (macOS-style: red/yellow/green) — left side
    comp_rect(w->x + 10, w->y + 8, 12, 12, t->danger);
    comp_rect(w->x + 28, w->y + 8, 12, 12, 0xE0B040);
    comp_rect(w->x + 46, w->y + 8, 12, 12, t->success);

    comp_text(w->x + 70, w->y + 6, w->title, t->text, tcol);
    comp_text(w->x + 14, w->y + TITLE_H + 10, w->body, t->text, t->win_body);
}

// ─── Top bar (Windows-style with clock on the right) ───
static void draw_topbar() {
    const theme_t* t = theme();
    comp_rect(0, 0, comp_width(), TOPBAR_H, t->taskbar_bg);
    comp_text(12, 6, "Vyro", t->accent_hi, t->taskbar_bg);
    comp_text(48, 6, "File  View  Window  Help",
              t->text_dim, t->taskbar_bg);

    // Right-aligned: user @ clock
    rtc_time_t rt; rtc_read(&rt);
    char clock[12] = "00:00:00";
    d2(clock,   rt.hour);
    d2(clock+3, rt.minute);
    d2(clock+6, rt.second);
    comp_text(comp_width() - 96, 6, clock, t->text, t->taskbar_bg);
}

// ─── Dock (macOS-style, bottom-center, with a glassy panel) ───
static int   hovered_dock = -1;
static void draw_dock(int mx, int my) {
    const theme_t* t = theme();
    int icon = 48, gap = 12, pad = 14;
    int dock_w = DOCK_ITEMS * icon + (DOCK_ITEMS - 1) * gap + pad * 2;
    int dock_x = (int)(comp_width() - dock_w) / 2;
    int dock_y = (int)comp_height() - DOCK_H - 8;

    comp_shadow(dock_x, dock_y, dock_w, DOCK_H, t->win_shadow);
    comp_rect(dock_x, dock_y, dock_w, DOCK_H, t->dock_bg);
    comp_border(dock_x, dock_y, dock_w, DOCK_H, t->dock_border);

    hovered_dock = -1;
    for (int i = 0; i < DOCK_ITEMS; i++) {
        int ix = dock_x + pad + i * (icon + gap);
        int iy = dock_y + 6;
        int hover = (mx >= ix && mx < ix + icon && my >= iy && my < iy + icon);
        if (hover) hovered_dock = i;
        uint32_t bg = hover ? t->accent : t->accent_hi;
        comp_rect(ix, iy, icon, icon, bg);
        comp_border(ix, iy, icon, icon, t->dock_border);
        // Icon glyph centered (double-draw at 4-pixel offset for fake "bold")
        comp_glyph(ix + 20, iy + 16, dock_icons[i], t->text_invert, bg);
        // Label appears only on hover (macOS-style tooltip above)
        if (hover) {
            const char* nm = dock_names[i];
            int len = 0; while (nm[len]) len++;
            int lx = ix + icon/2 - (len * 8) / 2;
            int ly = iy - 22;
            comp_rect(lx - 6, ly - 2, len*8 + 12, 20, t->win_body);
            comp_border(lx - 6, ly - 2, len*8 + 12, 20, t->win_border);
            comp_text(lx, ly + 2, nm, t->text, t->win_body);
        }
    }
}

// ─── Desktop background gradient ───
static void draw_desktop_bg() {
    const theme_t* t = theme();
    comp_gradient_v(0, 0, comp_width(), comp_height(), t->desktop_bg, t->desktop_bg2);
}

// ─── Arrow cursor (drawn into back buffer last so it's on top) ───
#define CW 12
#define CH 18
static const char* cursor_shape[CH] = {
    "X           ", "XX          ", "XwX         ", "XwwX        ",
    "XwwwX       ", "XwwwwX      ", "XwwwwwX     ", "XwwwwwwX    ",
    "XwwwwwwwX   ", "XwwwwwwwwX  ", "XwwwwwXXXXX ", "XwwXwwX     ",
    "XwX XwwX    ", "XX  XwwX    ", "X    XwwX   ", "     XwwX   ",
    "      XX    ", "            ",
};
static void draw_cursor(int x, int y) {
    for (int j = 0; j < CH; j++)
        for (int i = 0; i < CW; i++) {
            char c = cursor_shape[j][i];
            if (c == 'X') comp_pixel(x + i, y + j, 0x000000);
            else if (c == 'w') comp_pixel(x + i, y + j, 0xFFFFFF);
        }
}

// ─── Hit test helpers ───
static int point_in(int px, int py, int x, int y, int w, int h) {
    return px >= x && px < x + w && py >= y && py < y + h;
}
static void bring_to_front(int i) {
    if (i == win_count - 1) return;
    window_t tmp = wins[i];
    for (int k = i; k < win_count - 1; k++) wins[k] = wins[k+1];
    wins[win_count - 1] = tmp;
}

// ─── Full frame render ───
static void render(int mx, int my) {
    draw_desktop_bg();
    for (int i = 0; i < win_count; i++)
        if (wins[i].visible)
            draw_window(&wins[i], i == win_count - 1);
    draw_topbar();
    draw_dock(mx, my);
    draw_cursor(mx, my);
    comp_present();
}

// ─────────────────────────────────────────────────
// gui_run: the desktop event loop
// ─────────────────────────────────────────────────
void gui_run() {
    if (!fb_available()) return;
    if (!comp_init())    return;

    theme_init();

    wins[0] = (window_t){ 100, 90, 380, 220, "Welcome",
                          "Vyro OS 2.0 - drag title bar to move.", 1 };
    wins[1] = (window_t){ 540, 180, 380, 220, "About",
                          "64-bit OS, built from scratch. v2 desktop.", 1 };
    win_count = 2;

    int dragging = -1;
    int dox = 0, doy = 0;
    uint8_t prev_btn = 0;

    while (1) {
        if (keyboard_has_input()) {
            char c = keyboard_getchar();
            if (c == 0x1B) break;                     // ESC quits
            if (c == 't' || c == 'T')                 // 't' toggles theme
                theme_set_dark(!theme()->is_dark);
        }

        int mx = mouse_x(), my = mouse_y();
        uint8_t btn = mouse_buttons();
        int press = (btn & 1) && !(prev_btn & 1);
        int rel   = !(btn & 1) && (prev_btn & 1);

        if (press) {
            // Dock click?
            if (hovered_dock >= 0) {
                // Toggle a window for now (slot 0 = Welcome)
                if (hovered_dock < MAX_WINS && hovered_dock < win_count) {
                    wins[hovered_dock].visible = !wins[hovered_dock].visible;
                    if (wins[hovered_dock].visible) bring_to_front(hovered_dock);
                }
            } else {
                // Window hit test (front to back)
                for (int i = win_count - 1; i >= 0; i--) {
                    window_t* w = &wins[i];
                    if (!w->visible) continue;
                    // Close button (red traffic light)
                    if (point_in(mx, my, w->x + 10, w->y + 8, 12, 12)) {
                        w->visible = 0; break;
                    }
                    // Title bar drag
                    if (point_in(mx, my, w->x, w->y, w->w, TITLE_H)) {
                        bring_to_front(i);
                        dragging = win_count - 1;
                        dox = mx - wins[dragging].x;
                        doy = my - wins[dragging].y;
                        break;
                    }
                    // Body click → focus
                    if (point_in(mx, my, w->x, w->y, w->w, w->h)) {
                        bring_to_front(i); break;
                    }
                }
            }
        }
        if (rel) dragging = -1;

        if (dragging >= 0 && (btn & 1)) {
            window_t* w = &wins[dragging];
            int nx = mx - dox, ny = my - doy;
            if (ny < TOPBAR_H) ny = TOPBAR_H;
            w->x = nx; w->y = ny;
        }

        render(mx, my);
        prev_btn = btn;
        sleep_ms(16);   // ~60fps
    }
}
