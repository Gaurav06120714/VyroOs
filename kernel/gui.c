#include "gui.h"
#include "compositor.h"
#include "theme.h"
#include "icons.h"
#include "notify.h"
#include "../drivers/framebuffer.h"
#include "../drivers/mouse.h"
#include "../drivers/keyboard.h"
#include "../drivers/timer.h"
#include "../drivers/rtc.h"
#include "../include/types.h"

#define TOPBAR_H   28
#define DOCK_H     70
#define TITLE_H    26
#define MAX_WINS   6
#define DOCK_ITEMS 6

typedef struct {
    int x, y, w, h;
    const char* title;
    const char* body;
    uint8_t visible;
    uint64_t opened_at;     // for opening animation
} window_t;

static window_t wins[MAX_WINS];
static int      win_count = 0;
static int      hovered_dock = -1;

static const icon_t* dock_icons[DOCK_ITEMS] = {
    &ICON_FINDER, &ICON_TERMINAL, &ICON_SETTINGS, &ICON_BROWSER, &ICON_APPS, &ICON_TRASH
};

static void d2(char* buf, int n) { buf[0] = '0' + (n/10); buf[1] = '0' + (n%10); }

// ─── Window with shadow, traffic-lights, title, body ───
static void draw_window(window_t* w, int focused) {
    const theme_t* t = theme();

    // Opening animation: scale from 70% to 100% over ~12 frames
    uint64_t age = timer_ticks() - w->opened_at;
    int rx = w->x, ry = w->y, rw = w->w, rh = w->h;
    if (age < 12) {
        int scale = 70 + (int)age * 30 / 12;
        rw = w->w * scale / 100;
        rh = w->h * scale / 100;
        rx = w->x + (w->w - rw) / 2;
        ry = w->y + (w->h - rh) / 2;
    }

    comp_shadow(rx, ry, rw, rh, t->win_shadow);
    uint32_t tcol = focused ? t->win_title_focus : t->win_title;
    comp_rect(rx, ry, rw, TITLE_H, tcol);
    comp_rect(rx, ry + TITLE_H, rw, rh - TITLE_H, t->win_body);
    comp_border(rx, ry, rw, rh, t->win_border);

    if (age >= 12) {
        comp_rect(w->x + 10, w->y + 8, 12, 12, t->danger);
        comp_rect(w->x + 28, w->y + 8, 12, 12, 0xE0B040);
        comp_rect(w->x + 46, w->y + 8, 12, 12, t->success);
        comp_text(w->x + 70, w->y + 6, w->title, t->text, tcol);
        comp_text(w->x + 14, w->y + TITLE_H + 10, w->body, t->text, t->win_body);
    }
}

// ─── Top bar with live clock ───
static void draw_topbar() {
    const theme_t* t = theme();
    comp_rect(0, 0, comp_width(), TOPBAR_H, t->taskbar_bg);
    comp_text(12, 6, "Vyro", t->accent_hi, t->taskbar_bg);
    comp_text(48, 6, "File  View  Window  Help", t->text_dim, t->taskbar_bg);
    rtc_time_t rt; rtc_read(&rt);
    char clock[12] = "00:00:00";
    d2(clock,   rt.hour);  d2(clock+3, rt.minute);  d2(clock+6, rt.second);
    comp_text(comp_width() - 96, 6, clock, t->text, t->taskbar_bg);
}

// ─── Dock with real bitmap icons ───
static void draw_dock(int mx, int my) {
    const theme_t* t = theme();
    int icon_size = 48, gap = 14, pad = 16;
    int dock_w = DOCK_ITEMS * icon_size + (DOCK_ITEMS - 1) * gap + pad * 2;
    int dock_x = (comp_width() - dock_w) / 2;
    int dock_y = comp_height() - DOCK_H - 8;

    comp_shadow(dock_x, dock_y, dock_w, DOCK_H, t->win_shadow);
    comp_rect(dock_x, dock_y, dock_w, DOCK_H, t->dock_bg);
    comp_border(dock_x, dock_y, dock_w, DOCK_H, t->dock_border);

    hovered_dock = -1;
    for (int i = 0; i < DOCK_ITEMS; i++) {
        int ix = dock_x + pad + i * (icon_size + gap);
        int iy = dock_y + 10;
        int hover = (mx >= ix && mx < ix + icon_size && my >= iy && my < iy + icon_size);
        if (hover) hovered_dock = i;

        comp_rect(ix, iy, icon_size, icon_size, hover ? t->accent : t->win_body);
        comp_border(ix, iy, icon_size, icon_size, t->dock_border);
        icon_draw(dock_icons[i], ix + 8, iy + 8, 32);

        if (hover) {
            const char* nm = dock_icons[i]->name;
            int len = 0; while (nm[len]) len++;
            int lx = ix + icon_size/2 - (len * 8) / 2;
            int ly = iy - 24;
            comp_rect(lx - 8, ly - 4, len*8 + 16, 22, t->win_body);
            comp_border(lx - 8, ly - 4, len*8 + 16, 22, t->win_border);
            comp_text(lx, ly + 2, nm, t->text, t->win_body);
        }
    }
}

// ─── Notification toasts (top-right slide-in) ───
static void draw_notifications() {
    const theme_t* t = theme();
    notify_tick();
    notification_t* arr;
    int n = notify_active(&arr);
    int row = 0;
    for (int i = 0; i < 8 && row < 4; i++) {
        if (!arr[i].alive) continue;
        int nw = 320, nh = 64;
        int nx = comp_width() - nw - 16;
        int ny = TOPBAR_H + 12 + row * (nh + 10);
        comp_shadow(nx, ny, nw, nh, t->win_shadow);
        comp_rect(nx, ny, nw, nh, t->dock_bg);
        comp_rect(nx, ny, 5, nh, t->accent);
        comp_border(nx, ny, nw, nh, t->win_border);
        comp_text(nx + 14, ny + 10, arr[i].title, t->text, t->dock_bg);
        comp_text(nx + 14, ny + 34, arr[i].body,  t->text_dim, t->dock_bg);
        row++;
    }
}

// ─── Desktop background ───
static void draw_desktop_bg() {
    const theme_t* t = theme();
    comp_gradient_v(0, 0, comp_width(), comp_height(), t->desktop_bg, t->desktop_bg2);
}

// ─── Cursor ───
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

static int point_in(int px, int py, int x, int y, int w, int h) {
    return px >= x && px < x + w && py >= y && py < y + h;
}
static void bring_to_front(int i) {
    if (i == win_count - 1) return;
    window_t tmp = wins[i];
    for (int k = i; k < win_count - 1; k++) wins[k] = wins[k+1];
    wins[win_count - 1] = tmp;
}

static void render(int mx, int my) {
    draw_desktop_bg();
    for (int i = 0; i < win_count; i++)
        if (wins[i].visible) draw_window(&wins[i], i == win_count - 1);
    draw_topbar();
    draw_dock(mx, my);
    draw_notifications();
    draw_cursor(mx, my);
    comp_present();
}

void gui_run() {
    if (!fb_available()) return;
    if (!comp_init())    return;
    theme_init();

    wins[0] = (window_t){ 100, 90, 380, 220, "Welcome",
                          "Vyro OS 2.0 desktop - drag, click dock, T=theme", 1,
                          timer_ticks() };
    wins[1] = (window_t){ 540, 180, 380, 220, "About",
                          "64-bit OS - hover dock icons, ESC to exit", 1,
                          timer_ticks() };
    win_count = 2;

    notify_post("Welcome to Vyro OS 2.0", "Click dock icons to launch apps");

    int dragging = -1;
    int dox = 0, doy = 0;
    uint8_t prev_btn = 0;

    while (1) {
        if (keyboard_has_input()) {
            char c = keyboard_getchar();
            if (c == 0x1B) break;
            if (c == 't' || c == 'T') {
                theme_set_dark(!theme()->is_dark);
                notify_post("Theme switched", theme()->is_dark ? "Dark mode" : "Light mode");
            }
            if (c == 'n' || c == 'N')
                notify_post("Test notification", "You pressed N");
        }

        int mx = mouse_x(), my = mouse_y();
        uint8_t btn = mouse_buttons();
        int press = (btn & 1) && !(prev_btn & 1);
        int rel   = !(btn & 1) && (prev_btn & 1);

        if (press) {
            if (hovered_dock >= 0) {
                if (hovered_dock < win_count) {
                    wins[hovered_dock].visible = !wins[hovered_dock].visible;
                    if (wins[hovered_dock].visible) {
                        wins[hovered_dock].opened_at = timer_ticks();
                        bring_to_front(hovered_dock);
                    }
                }
                notify_post("Launched", dock_icons[hovered_dock]->name);
            } else {
                for (int i = win_count - 1; i >= 0; i--) {
                    window_t* w = &wins[i];
                    if (!w->visible) continue;
                    if (point_in(mx, my, w->x + 10, w->y + 8, 12, 12)) {
                        w->visible = 0; break;
                    }
                    if (point_in(mx, my, w->x, w->y, w->w, TITLE_H)) {
                        bring_to_front(i);
                        dragging = win_count - 1;
                        dox = mx - wins[dragging].x;
                        doy = my - wins[dragging].y;
                        break;
                    }
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
        sleep_ms(16);
    }
}
