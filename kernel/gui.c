#include "gui.h"
#include "compositor.h"
#include "theme.h"
#include "icons.h"
#include "notify.h"
#include "app.h"
#include "apps/apps.h"
#include "widgets_panel.h"
#include "../drivers/framebuffer.h"
#include "../drivers/mouse.h"
#include "../drivers/keyboard.h"
#include "../drivers/timer.h"
#include "../drivers/rtc.h"
#include "../include/types.h"

#define TOPBAR_H   28
#define DOCK_H     70
#define TITLE_H    26
#define MAX_WINS   8
#define DOCK_ITEMS 6
#define RESIZE_GRIP 14

typedef struct {
    const app_def_t* app;
    int x, y, w, h;
    int saved_x, saved_y, saved_w, saved_h;
    uint8_t visible;
    uint8_t minimized;
    uint8_t maximized;
    uint64_t opened_at;
    int last_key;
} window_t;

static window_t wins[MAX_WINS];
static int      win_count = 0;
static int      hovered_dock = -1;

static const char* dock_app_names[DOCK_ITEMS] = {
    "Files", "Terminal", "Settings", "Browser", "Launchpad", "Notifications"
};
static const icon_t* dock_icons[DOCK_ITEMS] = {
    &ICON_FINDER, &ICON_TERMINAL, &ICON_SETTINGS, &ICON_BROWSER, &ICON_APPS, &ICON_TRASH
};

static void d2(char* buf, int n) { buf[0] = '0' + (n/10); buf[1] = '0' + (n%10); }
static int point_in(int px, int py, int x, int y, int w, int h) {
    return px >= x && px < x + w && py >= y && py < y + h;
}
static void bring_to_front(int i) {
    if (i == win_count - 1) return;
    window_t tmp = wins[i];
    for (int k = i; k < win_count - 1; k++) wins[k] = wins[k+1];
    wins[win_count - 1] = tmp;
}

static int find_app_window(const char* name) {
    for (int i = 0; i < win_count; i++)
        if (wins[i].app) {
            const char* a = wins[i].app->name; const char* b = name;
            while (*a && *b && *a == *b) { a++; b++; }
            if (*a == *b) return i;
        }
    return -1;
}

static void open_app(const char* name) {
    int existing = find_app_window(name);
    if (existing >= 0) {
        wins[existing].visible = 1;
        wins[existing].minimized = 0;
        wins[existing].opened_at = timer_ticks();
        bring_to_front(existing);
        return;
    }
    const app_def_t* a = app_find(name);
    if (!a) return;
    if (win_count >= MAX_WINS) win_count = MAX_WINS - 1;
    window_t* w = &wins[win_count++];
    w->app = a;
    w->w = a->default_w; w->h = a->default_h + TITLE_H;
    w->x = 80 + (win_count * 30) % 200;
    w->y = TOPBAR_H + 30 + (win_count * 20) % 100;
    w->visible = 1; w->minimized = 0; w->maximized = 0;
    w->opened_at = timer_ticks();
    w->last_key = 0;
}

static void maximize_window(window_t* w) {
    if (!w->maximized) {
        w->saved_x = w->x; w->saved_y = w->y;
        w->saved_w = w->w; w->saved_h = w->h;
        w->x = 0; w->y = TOPBAR_H;
        w->w = comp_width(); w->h = comp_height() - TOPBAR_H - DOCK_H - 16;
        w->maximized = 1;
    } else {
        w->x = w->saved_x; w->y = w->saved_y;
        w->w = w->saved_w; w->h = w->saved_h;
        w->maximized = 0;
    }
}

// ─── Window draw ───
static void draw_window(window_t* w, int focused, int mx, int my, int clicked) {
    const theme_t* t = theme();
    if (w->minimized || !w->visible || !w->app) return;

    uint64_t age = timer_ticks() - w->opened_at;
    int rx = w->x, ry = w->y, rw = w->w, rh = w->h;
    if (age < 8) {
        int scale = 80 + (int)age * 20 / 8;
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

    if (age >= 8) {
        comp_rect(w->x + 10, w->y + 8, 12, 12, t->danger);
        comp_rect(w->x + 28, w->y + 8, 12, 12, 0xE0B040);
        comp_rect(w->x + 46, w->y + 8, 12, 12, t->success);
        comp_text(w->x + 70, w->y + 6, w->app->name, t->text, tcol);
        if (!w->maximized) {
            for (int i = 0; i < 8; i++) {
                comp_pixel(w->x + w->w - 3 - i, w->y + w->h - 3, t->text_dim);
                comp_pixel(w->x + w->w - 3,     w->y + w->h - 3 - i, t->text_dim);
            }
        }

        // Call app's render function
        int body_x = w->x;
        int body_y = w->y + TITLE_H;
        int body_w = w->w;
        int body_h = w->h - TITLE_H;
        int body_mx = mx - body_x;
        int body_my = my - body_y;
        int in_body = (body_mx >= 0 && body_mx < body_w && body_my >= 0 && body_my < body_h);
        app_ctx_t ctx = {
            .mx = body_mx, .my = body_my,
            .btn = 0,
            .clicked = (in_body && focused && clicked) ? 1 : 0,
            .key = (focused ? w->last_key : 0),
            .width = body_w, .height = body_h,
            .origin_x = body_x, .origin_y = body_y
        };
        w->app->render(&ctx);
        w->last_key = 0;
    }
}

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

        int wi = find_app_window(dock_app_names[i]);
        if (wi >= 0 && wins[wi].visible) {
            int dot_x = ix + icon_size/2 - 2;
            int dot_y = iy + icon_size + 4;
            comp_rect(dot_x, dot_y, 4, 4, t->accent_hi);
        }
        if (hover) {
            const char* nm = dock_app_names[i];
            int len = 0; while (nm[len]) len++;
            int lx = ix + icon_size/2 - (len * 8) / 2;
            int ly = iy - 24;
            comp_rect(lx - 8, ly - 4, len*8 + 16, 22, t->win_body);
            comp_border(lx - 8, ly - 4, len*8 + 16, 22, t->win_border);
            comp_text(lx, ly + 2, nm, t->text, t->win_body);
        }
    }
}

static void draw_notifications() {
    const theme_t* t = theme();
    notify_tick();
    notification_t* arr;
    notify_active(&arr);
    int row = 0;
    for (int i = 0; i < 8 && row < 4; i++) {
        if (!arr[i].alive) continue;
        int nw = 320, nh = 56;
        int nx = comp_width() - nw - 16;
        int ny = TOPBAR_H + 12 + row * (nh + 8);
        comp_shadow(nx, ny, nw, nh, t->win_shadow);
        comp_rect(nx, ny, nw, nh, t->dock_bg);
        comp_rect(nx, ny, 5, nh, t->accent);
        comp_border(nx, ny, nw, nh, t->win_border);
        comp_text(nx + 14, ny + 8, arr[i].title, t->text, t->dock_bg);
        comp_text(nx + 14, ny + 28, arr[i].body, t->text_dim, t->dock_bg);
        row++;
    }
}

static void draw_desktop_bg() {
    const theme_t* t = theme();
    comp_gradient_v(0, 0, comp_width(), comp_height(), t->desktop_bg, t->desktop_bg2);
}

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

static void render(int mx, int my, int clicked) {
    draw_desktop_bg();
    for (int i = 0; i < win_count; i++)
        draw_window(&wins[i], i == win_count - 1, mx, my, clicked);
    draw_topbar();
    // Right-side widgets panel (320px wide, like macOS Sequoia)
    widgets_panel_draw(comp_width() - 320, TOPBAR_H + 8, 320);
    draw_dock(mx, my);
    draw_notifications();
    draw_cursor(mx, my);
    comp_present();
}

// Launcher callback
extern void launcher_set_callback(void (*cb)(const char*));
static void on_launch(const char* name) { open_app(name); }

void gui_run() {
    if (!fb_available()) return;
    if (!comp_init())    return;
    theme_init();
    apps_register_all();
    launcher_set_callback(on_launch);

    open_app("Files");
    open_app("Clock");
    notify_post("Vyro OS 2.0", "Desktop ready - click dock or Launchpad");

    int dragging = -1, resizing = -1;
    int dox = 0, doy = 0;
    uint8_t prev_btn = 0;

    while (1) {
        int last_key = 0;
        if (keyboard_has_input()) {
            char c = keyboard_getchar();
            if (c == 0x1B) break;
            if (c == 't' || c == 'T') { theme_set_dark(!theme()->is_dark); notify_post("Theme", theme()->is_dark?"Dark":"Light"); }
            else last_key = (int)(unsigned char)c;
        }

        // Forward key to focused window
        if (last_key && win_count > 0)
            wins[win_count - 1].last_key = last_key;

        int mx = mouse_x(), my = mouse_y();
        uint8_t btn = mouse_buttons();
        int press = (btn & 1) && !(prev_btn & 1);
        int rel   = !(btn & 1) && (prev_btn & 1);

        if (press) {
            if (hovered_dock >= 0) {
                open_app(dock_app_names[hovered_dock]);
            } else {
                for (int i = win_count - 1; i >= 0; i--) {
                    window_t* w = &wins[i];
                    if (!w->visible || w->minimized) continue;
                    if (point_in(mx, my, w->x + 10, w->y + 8, 12, 12)) { w->visible = 0; break; }
                    if (point_in(mx, my, w->x + 28, w->y + 8, 12, 12)) { w->minimized = 1; break; }
                    if (point_in(mx, my, w->x + 46, w->y + 8, 12, 12)) { maximize_window(w); break; }
                    if (!w->maximized && point_in(mx, my, w->x + w->w - RESIZE_GRIP,
                            w->y + w->h - RESIZE_GRIP, RESIZE_GRIP, RESIZE_GRIP)) {
                        bring_to_front(i); resizing = win_count - 1;
                        dox = mx - (w->x + w->w); doy = my - (w->y + w->h);
                        break;
                    }
                    if (point_in(mx, my, w->x, w->y, w->w, TITLE_H)) {
                        bring_to_front(i); dragging = win_count - 1;
                        dox = mx - wins[dragging].x; doy = my - wins[dragging].y;
                        break;
                    }
                    if (point_in(mx, my, w->x, w->y, w->w, w->h)) {
                        bring_to_front(i); break;
                    }
                }
            }
        }
        if (rel) { dragging = -1; resizing = -1; }

        if (dragging >= 0 && (btn & 1)) {
            window_t* w = &wins[dragging];
            int nx = mx - dox, ny = my - doy;
            if (ny < TOPBAR_H) ny = TOPBAR_H;
            w->x = nx; w->y = ny; w->maximized = 0;
        }
        if (resizing >= 0 && (btn & 1)) {
            window_t* w = &wins[resizing];
            int nw = mx - dox - w->x; int nh = my - doy - w->y;
            if (nw < 240) nw = 240; if (nh < 160) nh = 160;
            w->w = nw; w->h = nh;
        }

        render(mx, my, press);
        prev_btn = btn;
        sleep_ms(16);
    }
}
