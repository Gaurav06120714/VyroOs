#include "gui.h"
#include "../drivers/framebuffer.h"
#include "../drivers/mouse.h"
#include "../drivers/keyboard.h"
#include "../drivers/timer.h"
#include "../include/types.h"

#define DESKTOP_BG   FB_COLOR(30, 40, 70)
#define TASKBAR_BG   FB_COLOR(20, 20, 30)
#define WIN_TITLE    FB_COLOR(70, 90, 160)
#define WIN_TITLE_HI FB_COLOR(90, 120, 210)   // focused window title
#define WIN_BODY     FB_COLOR(235, 235, 240)
#define WIN_TEXT     FB_COLOR(20, 20, 30)
#define MENU_BG      FB_COLOR(45, 50, 70)
#define MENU_HI      FB_COLOR(70, 90, 160)

#define TITLE_H   22
#define MAX_WINS  4
#define TASKBAR_H 28

typedef struct {
    int x, y, w, h;
    const char* title;
    const char* body;
    uint8_t visible;
} window_t;

// Windows in z-order: index 0 = back, last = front
static window_t wins[MAX_WINS];
static int      win_count = 0;

static uint8_t  menu_open = 0;

// Cursor
#define CURSOR_W 12
#define CURSOR_H 18
static uint32_t cursor_save[CURSOR_W * CURSOR_H];
static int      last_cx = -1, last_cy = -1;

static const char* cursor_shape[CURSOR_H] = {
    "X           ", "XX          ", "XwX         ", "XwwX        ",
    "XwwwX       ", "XwwwwX      ", "XwwwwwX     ", "XwwwwwwX    ",
    "XwwwwwwwX   ", "XwwwwwwwwX  ", "XwwwwwXXXXX ", "XwwXwwX     ",
    "XwX XwwX    ", "XX  XwwX    ", "X    XwwX   ", "     XwwX   ",
    "      XX    ", "            ",
};

// ─────────────────────────────────────────────────
// Window drawing
// ─────────────────────────────────────────────────
static void draw_window(window_t* win, int focused) {
    uint32_t tcol = focused ? WIN_TITLE_HI : WIN_TITLE;
    fb_fill_rect(win->x, win->y, win->w, TITLE_H, tcol);
    fb_fill_rect(win->x, win->y + TITLE_H, win->w, win->h - TITLE_H, WIN_BODY);
    fb_draw_rect(win->x, win->y, win->w, win->h, FB_BLACK);
    fb_draw_text(win->x + 6, win->y + 4, win->title, FB_WHITE, tcol);
    // close button
    fb_fill_rect(win->x + win->w - 18, win->y + 4, 14, 14, FB_RED);
    fb_draw_text(win->x + win->w - 15, win->y + 4, "x", FB_WHITE, FB_RED);
    // body
    fb_draw_text(win->x + 8, win->y + TITLE_H + 10, win->body, WIN_TEXT, WIN_BODY);
}

// ─────────────────────────────────────────────────
// Full scene render
// ─────────────────────────────────────────────────
static void draw_scene() {
    fb_clear(DESKTOP_BG);

    // Top bar
    fb_fill_rect(0, 0, FB_WIDTH, 24, TASKBAR_BG);
    fb_draw_text(8, 4, "Vyro OS  Window Manager", FB_CYAN, TASKBAR_BG);
    fb_draw_text(FB_WIDTH - 280, 4, "Drag titles - ESC exits", FB_GREY, TASKBAR_BG);

    // Windows back-to-front; front (last) is focused
    for (int i = 0; i < win_count; i++) {
        if (wins[i].visible)
            draw_window(&wins[i], i == win_count - 1);
    }

    // Bottom taskbar
    fb_fill_rect(0, FB_HEIGHT - TASKBAR_H, FB_WIDTH, TASKBAR_H, TASKBAR_BG);
    uint32_t scol = menu_open ? MENU_HI : WIN_TITLE;
    fb_fill_rect(6, FB_HEIGHT - 24, 80, 20, scol);
    fb_draw_text(16, FB_HEIGHT - 22, "Start", FB_WHITE, scol);

    // Start menu
    if (menu_open) {
        int mx = 6, my = FB_HEIGHT - TASKBAR_H - 90, mw = 160, mh = 90;
        fb_fill_rect(mx, my, mw, mh, MENU_BG);
        fb_draw_rect(mx, my, mw, mh, FB_BLACK);
        fb_draw_text(mx + 10, my + 8,  "Show Welcome", FB_WHITE, MENU_BG);
        fb_draw_text(mx + 10, my + 36, "Show About",   FB_WHITE, MENU_BG);
        fb_draw_text(mx + 10, my + 64, "Hide All",     FB_WHITE, MENU_BG);
    }
}

// ─────────────────────────────────────────────────
// Cursor save/restore/draw
// ─────────────────────────────────────────────────
static void cursor_save_under(int mx, int my) {
    for (int j = 0; j < CURSOR_H; j++)
        for (int i = 0; i < CURSOR_W; i++)
            cursor_save[j * CURSOR_W + i] = fb_get_pixel(mx + i, my + j);
}
static void cursor_restore(int mx, int my) {
    for (int j = 0; j < CURSOR_H; j++)
        for (int i = 0; i < CURSOR_W; i++)
            fb_putpixel(mx + i, my + j, cursor_save[j * CURSOR_W + i]);
}
static void cursor_draw(int mx, int my) {
    for (int j = 0; j < CURSOR_H; j++)
        for (int i = 0; i < CURSOR_W; i++) {
            char c = cursor_shape[j][i];
            if (c == 'X') fb_putpixel(mx + i, my + j, FB_BLACK);
            else if (c == 'w') fb_putpixel(mx + i, my + j, FB_WHITE);
        }
}

// Repaint whole scene, then place cursor freshly
static void full_repaint(int cx, int cy) {
    draw_scene();
    cursor_save_under(cx, cy);
    cursor_draw(cx, cy);
    last_cx = cx;
    last_cy = cy;
}

// ─────────────────────────────────────────────────
// Hit testing
// ─────────────────────────────────────────────────
static int point_in(int px, int py, int x, int y, int w, int h) {
    return px >= x && px < x + w && py >= y && py < y + h;
}

// Bring window at index i to front (focus)
static void bring_to_front(int i) {
    if (i == win_count - 1) return;
    window_t tmp = wins[i];
    for (int k = i; k < win_count - 1; k++) wins[k] = wins[k + 1];
    wins[win_count - 1] = tmp;
}

// ─────────────────────────────────────────────────
// gui_run: window manager event loop
// ─────────────────────────────────────────────────
void gui_run() {
    if (!fb_available()) return;

    // Initialize windows
    wins[0] = (window_t){ 120, 120, 360, 200, "Welcome",
                          "Vyro OS desktop - drag me by my title bar!", 1 };
    wins[1] = (window_t){ 540, 220, 360, 200, "About",
                          "64-bit OS, built from scratch. $0 budget.", 1 };
    win_count = 2;
    menu_open = 0;

    int dragging = -1;       // window index being dragged
    int drag_off_x = 0, drag_off_y = 0;
    uint8_t prev_btn = 0;
    int prev_mx = -1, prev_my = -1;

    full_repaint(mouse_x(), mouse_y());

    while (1) {
        if (keyboard_has_input()) {
            if (keyboard_getchar() == 0x1B) break;   // ESC
        }

        int mx = mouse_x();
        int my = mouse_y();
        uint8_t btn = mouse_buttons();

        uint8_t left_press   = (btn & 1) && !(prev_btn & 1);
        uint8_t left_release = !(btn & 1) && (prev_btn & 1);
        int     need_repaint = 0;

        // ── Left button pressed: dispatch click ──
        if (left_press) {
            // Start button?
            if (point_in(mx, my, 6, FB_HEIGHT - 24, 80, 20)) {
                menu_open = !menu_open;
                need_repaint = 1;
            }
            // Start menu item?
            else if (menu_open) {
                int menux = 6, menuy = FB_HEIGHT - TASKBAR_H - 90;
                if (point_in(mx, my, menux, menuy, 160, 90)) {
                    int item = (my - menuy) / 28;
                    if (item == 0) wins[0].visible = 1, bring_to_front(0);
                    else if (item == 1) wins[1].visible = 1, bring_to_front(1);
                    else if (item == 2) { wins[0].visible = 0; wins[1].visible = 0; }
                    menu_open = 0;
                    need_repaint = 1;
                } else {
                    menu_open = 0;
                    need_repaint = 1;
                }
            }
            else {
                // Check windows front-to-back
                for (int i = win_count - 1; i >= 0; i--) {
                    window_t* w = &wins[i];
                    if (!w->visible) continue;

                    // Close button?
                    if (point_in(mx, my, w->x + w->w - 18, w->y + 4, 14, 14)) {
                        w->visible = 0;
                        need_repaint = 1;
                        break;
                    }
                    // Title bar → start drag + focus
                    if (point_in(mx, my, w->x, w->y, w->w, TITLE_H)) {
                        bring_to_front(i);
                        dragging  = win_count - 1;
                        drag_off_x = mx - wins[win_count - 1].x;
                        drag_off_y = my - wins[win_count - 1].y;
                        need_repaint = 1;
                        break;
                    }
                    // Body click → just focus
                    if (point_in(mx, my, w->x, w->y, w->w, w->h)) {
                        bring_to_front(i);
                        need_repaint = 1;
                        break;
                    }
                }
            }
        }

        if (left_release) dragging = -1;

        // ── Dragging a window ──
        if (dragging >= 0 && (btn & 1)) {
            window_t* w = &wins[dragging];
            int nx = mx - drag_off_x;
            int ny = my - drag_off_y;
            if (ny < 24) ny = 24;   // keep below top bar
            if (nx != w->x || ny != w->y) {
                w->x = nx;
                w->y = ny;
                need_repaint = 1;
            }
        }

        // ── Render ──
        if (need_repaint) {
            full_repaint(mx, my);
        } else if (mx != prev_mx || my != prev_my) {
            // Just the cursor moved
            if (last_cx >= 0) cursor_restore(last_cx, last_cy);
            cursor_save_under(mx, my);
            cursor_draw(mx, my);
            last_cx = mx;
            last_cy = my;
        }

        prev_btn = btn;
        prev_mx  = mx;
        prev_my  = my;
        sleep_ms(8);
    }
}
