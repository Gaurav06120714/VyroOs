#include "gui.h"
#include "compositor.h"
#include "theme.h"
#include "icons.h"
#include "notify.h"
#include "app.h"
#include "apps/apps.h"
#include "widgets_panel.h"
#include "widgets.h"
#include "ctxmenu.h"
#include "security.h"
#include "sha256.h"
#include "power.h"
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
#define NUM_DESKTOPS 4

typedef struct {
    const app_def_t* app;
    int x, y, w, h;
    int saved_x, saved_y, saved_w, saved_h;
    uint8_t visible;
    uint8_t minimized;
    uint8_t maximized;
    uint8_t desktop;       // virtual desktop index
    uint64_t opened_at;
    int last_key;
} window_t;

static window_t wins[MAX_WINS];
static int      win_count = 0;
static int      hovered_dock = -1;
static int      current_desktop = 0;

// UI state
static ctxmenu_t menu;
static int       show_power_menu = 0;
static int       show_quick_settings = 0;
static int       locked = 0;
static char      pw_buf[32];
static int       pw_len = 0;
static int       pw_wrong = 0;

// Context source — what was right-clicked (for action dispatch)
typedef enum { CTX_NONE, CTX_DESKTOP, CTX_WINDOW, CTX_DOCK } ctx_source_t;
static ctx_source_t ctx_source = CTX_NONE;
static int          ctx_window_idx = -1;
static int          ctx_dock_idx = -1;

// Action IDs
#define ACT_NEW_FOLDER      100
#define ACT_CHANGE_WALL     101
#define ACT_TOGGLE_THEME    102
#define ACT_SHOW_DESKTOP    103
#define ACT_OPEN_SETTINGS   104

#define ACT_WIN_MIN         200
#define ACT_WIN_MAX         201
#define ACT_WIN_CLOSE       202
#define ACT_WIN_SEND_DESK1  210
#define ACT_WIN_SEND_DESK2  211
#define ACT_WIN_SEND_DESK3  212
#define ACT_WIN_SEND_DESK4  213

#define ACT_DOCK_OPEN       300
#define ACT_DOCK_PIN        301
#define ACT_DOCK_QUIT       302

#define ACT_POWER_SLEEP     400
#define ACT_POWER_LOCK      401
#define ACT_POWER_LOGOUT    402
#define ACT_POWER_RESTART   403
#define ACT_POWER_SHUTDOWN  404

static const char* dock_app_names[DOCK_ITEMS] = {
    "Files", "Terminal", "Settings", "Browser", "Launchpad", "Notifications"
};
static const icon_t* dock_icons[DOCK_ITEMS] = {
    &ICON_FINDER, &ICON_TERMINAL, &ICON_SETTINGS, &ICON_BROWSER, &ICON_APPS, &ICON_TRASH
};

// Wallpaper presets (gradient pairs)
static uint32_t wallpapers[][2] = {
    { 0x161C2C, 0x0C101C },     // default dark blue
    { 0x2A1530, 0x16081A },     // purple
    { 0x102822, 0x081410 },     // forest
    { 0x301010, 0x180808 },     // sunset red
    { 0xDDE4F0, 0xC0CCDE },     // light
};
#define NUM_WALLPAPERS 5
static int current_wall = 0;

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
        wins[existing].desktop = current_desktop;
        bring_to_front(existing);
        return;
    }
    const app_def_t* a = app_find(name);
    if (!a) return;
    if (win_count >= MAX_WINS) win_count = MAX_WINS - 1;
    window_t* w = &wins[win_count++];
    w->app = a;
    int max_w = comp_width() - 320 - 80;
    int max_h = comp_height() - TOPBAR_H - DOCK_H - 60;
    int dw_ = a->default_w; if (dw_ > max_w) dw_ = max_w;
    int dh_ = a->default_h + TITLE_H; if (dh_ > max_h) dh_ = max_h;
    w->w = dw_; w->h = dh_;
    w->x = 60 + (win_count * 30) % 80;
    w->y = TOPBAR_H + 40 + (win_count * 20) % 80;
    w->visible = 1; w->minimized = 0; w->maximized = 0;
    w->opened_at = timer_ticks();
    w->last_key = 0;
    w->desktop = current_desktop;
}
static void maximize_window(window_t* w) {
    if (!w->maximized) {
        w->saved_x = w->x; w->saved_y = w->y;
        w->saved_w = w->w; w->saved_h = w->h;
        w->x = 0; w->y = TOPBAR_H;
        w->w = comp_width() - 320; w->h = comp_height() - TOPBAR_H - DOCK_H - 16;
        w->maximized = 1;
    } else {
        w->x = w->saved_x; w->y = w->saved_y;
        w->w = w->saved_w; w->h = w->saved_h;
        w->maximized = 0;
    }
}

// ─── Render functions ───
static void draw_window(window_t* w, int focused, int mx, int my, int clicked) {
    const theme_t* t = theme();
    if (w->minimized || !w->visible || !w->app) return;
    if (w->desktop != current_desktop) return;

    uint64_t age = timer_ticks() - w->opened_at;
    int rx = w->x, ry = w->y, rw = w->w, rh = w->h;
    if (age < 8) {
        int scale = 80 + (int)age * 20 / 8;
        rw = w->w * scale / 100; rh = w->h * scale / 100;
        rx = w->x + (w->w - rw) / 2; ry = w->y + (w->h - rh) / 2;
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
        int body_x = w->x, body_y = w->y + TITLE_H;
        int body_w = w->w, body_h = w->h - TITLE_H;
        int body_mx = mx - body_x, body_my = my - body_y;
        int in_body = (body_mx >= 0 && body_mx < body_w && body_my >= 0 && body_my < body_h);
        app_ctx_t ctx = {
            .mx = body_mx, .my = body_my, .btn = 0,
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

    // Right-side icons: workspace indicator + user + power
    char wsbuf[8] = "[ ] ";
    wsbuf[1] = '1' + current_desktop;
    comp_text(comp_width() - 300, 6, wsbuf, t->text_dim, t->taskbar_bg);

    comp_text(comp_width() - 250, 6, current_user(), t->text, t->taskbar_bg);

    // Settings/power icon area (clickable region)
    comp_text(comp_width() - 168, 6, "[*]", t->text, t->taskbar_bg);   // quick settings
    comp_text(comp_width() - 140, 6, "[u]", t->text, t->taskbar_bg);   // user
    comp_text(comp_width() - 112, 6, "[P]", t->text, t->taskbar_bg);   // power

    rtc_time_t rt; rtc_read(&rt);
    char clock[12] = "00:00:00";
    d2(clock,   rt.hour);  d2(clock+3, rt.minute);  d2(clock+6, rt.second);
    comp_text(comp_width() - 80, 6, clock, t->text, t->taskbar_bg);
}

static void draw_dock(int mx, int my) {
    const theme_t* t = theme();
    int icon_size = 48, gap = 14, pad = 16;
    int dock_w = DOCK_ITEMS * icon_size + (DOCK_ITEMS - 1) * gap + pad * 2;
    int usable_w = comp_width() - 320;
    int dock_x = (usable_w - dock_w) / 2;
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
    notification_t* arr; notify_active(&arr);
    int row = 0;
    for (int i = 0; i < 8 && row < 4; i++) {
        if (!arr[i].alive) continue;
        int nw = 300, nh = 56;
        int nx = comp_width() - 320 - nw - 16;
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
    comp_gradient_v(0, 0, comp_width(), comp_height(),
                    wallpapers[current_wall][0], wallpapers[current_wall][1]);
    // Workspace pager dots
    int cx = (comp_width() - 320) / 2;
    int cy = comp_height() - DOCK_H - 32;
    for (int i = 0; i < NUM_DESKTOPS; i++) {
        int dx = cx - (NUM_DESKTOPS * 14) / 2 + i * 14;
        uint32_t c = (i == current_desktop) ? theme()->accent_hi : theme()->win_border;
        comp_rect(dx, cy, 6, 6, c);
    }
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

// ─── Lock screen ───
static void draw_lockscreen(int mx, int my) {
    (void)mx; (void)my;
    comp_gradient_v(0, 0, comp_width(), comp_height(), 0x0A0E18, 0x000000);
    const theme_t* t = theme();
    // Clock big-centre
    rtc_time_t rt; rtc_read(&rt);
    char clk[6]; d2(clk, rt.hour); clk[2] = ':'; d2(clk+3, rt.minute); clk[5] = 0;
    int cx = comp_width()/2 - 90, cy = 140;
    for (int i = 0; i < 5; i++) {
        for (int dy = 0; dy < 4; dy++)
            for (int dx = 0; dx < 4; dx++)
                comp_glyph(cx + i * 32 + dx, cy + dy, clk[i], 0xFFFFFF, 0);
    }
    comp_text(comp_width()/2 - 40, cy + 80, "Vyro OS 2.0", 0xA0B0C0, 0);

    // Login panel
    int pw = 400, ph = 200, px = (comp_width()-pw)/2, py = comp_height()/2 + 20;
    comp_rect(px, py, pw, ph, 0x20283C);
    comp_border(px, py, pw, ph, 0x3A4258);
    comp_text(px + 20, py + 16, "Sign in", 0xFFFFFF, 0x20283C);
    comp_text(px + 20, py + 50, "User:", t->text_dim, 0x20283C);
    comp_text(px + 80, py + 50, current_user(), 0xFFFFFF, 0x20283C);
    comp_text(px + 20, py + 80, "Password:", t->text_dim, 0x20283C);
    // Password field
    comp_rect(px + 20, py + 100, pw - 40, 30, 0x10141E);
    comp_border(px + 20, py + 100, pw - 40, 30, 0x3A4258);
    // Show dots for entered chars
    for (int i = 0; i < pw_len && i < 30; i++) {
        comp_rect(px + 28 + i * 12, py + 112, 8, 8, 0xFFFFFF);
    }
    // Hint
    if (pw_wrong)
        comp_text(px + 20, py + 140, "Wrong password. Try again.", 0xFF7070, 0x20283C);
    else
        comp_text(px + 20, py + 140, "Press Enter to unlock. (default: guest)", t->text_dim, 0x20283C);
    comp_text(px + 20, py + 160, "Tip: type 'guest' then Enter", t->text_dim, 0x20283C);

    draw_cursor(mx, my);
    comp_present();
}

// ─── Power menu ───
static void draw_power_menu(int mx, int my) {
    if (!show_power_menu) return;
    const theme_t* t = theme();
    int pw = 200, ph = 178, px = comp_width() - 220, py = TOPBAR_H + 4;
    comp_shadow(px, py, pw, ph, t->win_shadow);
    comp_rect(px, py, pw, ph, t->dock_bg);
    comp_border(px, py, pw, ph, t->win_border);
    const char* items[] = { "Sleep", "Lock Screen", "Log Out", "Restart", "Shut Down" };
    int actions[] = { ACT_POWER_SLEEP, ACT_POWER_LOCK, ACT_POWER_LOGOUT, ACT_POWER_RESTART, ACT_POWER_SHUTDOWN };
    for (int i = 0; i < 5; i++) {
        int ry = py + 6 + i * 32;
        int hover = (mx >= px && mx < px + pw && my >= ry && my < ry + 28);
        if (hover) comp_rect(px + 2, ry, pw - 4, 28, t->accent);
        uint32_t col = (i == 4 || i == 3) ? 0xFF8080 : t->text;
        if (hover) col = 0xFFFFFF;
        comp_text(px + 16, ry + 6, items[i], col, hover ? t->accent : t->dock_bg);
        (void)actions;
    }
}

// ─── Quick settings ───
static void draw_quick_settings(int mx, int my) {
    if (!show_quick_settings) return;
    const theme_t* t = theme();
    int pw = 280, ph = 280, px = comp_width() - 300, py = TOPBAR_H + 4;
    comp_shadow(px, py, pw, ph, t->win_shadow);
    comp_rect(px, py, pw, ph, t->dock_bg);
    comp_border(px, py, pw, ph, t->win_border);
    comp_text(px + 16, py + 10, "Quick Settings", t->accent_hi, t->dock_bg);

    const char* labels[] = { "Wi-Fi", "Bluetooth", "Do Not Disturb", "AirDrop" };
    static int states[4] = { 1, 0, 0, 1 };
    int row_y = py + 40;
    for (int i = 0; i < 4; i++) {
        comp_text(px + 16, row_y + 4, labels[i], t->text, t->dock_bg);
        int new_state = w_toggle(px + pw - 60, row_y, states[i], mx, my, 0);
        states[i] = new_state;
        row_y += 30;
    }

    // Brightness + Volume sliders
    comp_text(px + 16, row_y + 6, "Brightness", t->text, t->dock_bg);
    w_progress(px + 16, row_y + 24, pw - 32, 12, 80);
    row_y += 50;
    comp_text(px + 16, row_y + 6, "Volume", t->text, t->dock_bg);
    w_progress(px + 16, row_y + 24, pw - 32, 12, 60);
}

static void render(int mx, int my, int clicked) {
    if (locked) { draw_lockscreen(mx, my); return; }

    draw_desktop_bg();
    for (int i = 0; i < win_count; i++)
        draw_window(&wins[i], i == win_count - 1, mx, my, clicked);
    draw_topbar();
    widgets_panel_draw(comp_width() - 320, TOPBAR_H + 8, 320);
    draw_dock(mx, my);
    draw_notifications();
    draw_power_menu(mx, my);
    draw_quick_settings(mx, my);
    ctxmenu_draw(&menu, mx, my);
    draw_cursor(mx, my);
    comp_present();
}

// Launcher callback
extern void launcher_set_callback(void (*cb)(const char*));
static void on_launch(const char* name) { open_app(name); }

// ─── Lock screen input ───
static void lockscreen_handle_key(char c) {
    if (c == '\n') {
        // Try to log in with typed password
        pw_buf[pw_len] = '\0';
        if (auth_login(current_user(), pw_buf) == 0) {
            locked = 0; pw_len = 0; pw_buf[0] = 0; pw_wrong = 0;
            notify_post("Welcome back", current_user());
        } else {
            pw_wrong = 1; pw_len = 0; pw_buf[0] = 0;
        }
    } else if (c == '\b') {
        if (pw_len > 0) pw_buf[--pw_len] = 0;
    } else if (c >= 32 && c < 127 && pw_len < 30) {
        pw_buf[pw_len++] = c;
    }
}

// ─── Build context menus ───
static void build_desktop_menu() {
    ctxmenu_clear(&menu);
    ctxmenu_add(&menu, "New Folder",         ACT_NEW_FOLDER,    CTX_ITEM_NORMAL);
    ctxmenu_add(&menu, "",                   0,                 CTX_ITEM_SEP);
    ctxmenu_add(&menu, "Change Wallpaper",   ACT_CHANGE_WALL,   CTX_ITEM_NORMAL);
    ctxmenu_add(&menu, "Toggle Theme",       ACT_TOGGLE_THEME,  CTX_ITEM_NORMAL);
    ctxmenu_add(&menu, "",                   0,                 CTX_ITEM_SEP);
    ctxmenu_add(&menu, "Show Desktop",       ACT_SHOW_DESKTOP,  CTX_ITEM_NORMAL);
    ctxmenu_add(&menu, "Open Settings",      ACT_OPEN_SETTINGS, CTX_ITEM_NORMAL);
}
static void build_window_menu() {
    ctxmenu_clear(&menu);
    ctxmenu_add(&menu, "Minimize",            ACT_WIN_MIN,        CTX_ITEM_NORMAL);
    ctxmenu_add(&menu, "Toggle Maximize",     ACT_WIN_MAX,        CTX_ITEM_NORMAL);
    ctxmenu_add(&menu, "",                    0,                  CTX_ITEM_SEP);
    ctxmenu_add(&menu, "Send to Desktop 1",   ACT_WIN_SEND_DESK1, CTX_ITEM_NORMAL);
    ctxmenu_add(&menu, "Send to Desktop 2",   ACT_WIN_SEND_DESK2, CTX_ITEM_NORMAL);
    ctxmenu_add(&menu, "Send to Desktop 3",   ACT_WIN_SEND_DESK3, CTX_ITEM_NORMAL);
    ctxmenu_add(&menu, "Send to Desktop 4",   ACT_WIN_SEND_DESK4, CTX_ITEM_NORMAL);
    ctxmenu_add(&menu, "",                    0,                  CTX_ITEM_SEP);
    ctxmenu_add(&menu, "Close",               ACT_WIN_CLOSE,      CTX_ITEM_DANGER);
}
static void build_dock_menu(int idx) {
    ctxmenu_clear(&menu);
    ctxmenu_add(&menu, "Open",               ACT_DOCK_OPEN,  CTX_ITEM_NORMAL);
    ctxmenu_add(&menu, "Pin to Dock",        ACT_DOCK_PIN,   CTX_ITEM_DISABLED);
    ctxmenu_add(&menu, "",                   0,              CTX_ITEM_SEP);
    int wi = find_app_window(dock_app_names[idx]);
    if (wi >= 0 && wins[wi].visible)
        ctxmenu_add(&menu, "Quit App",       ACT_DOCK_QUIT,  CTX_ITEM_DANGER);
}

// ─── Dispatch context menu action ───
static void dispatch_action(int act) {
    switch (act) {
        case ACT_NEW_FOLDER:
            notify_post("New Folder", "Created (simulated)"); break;
        case ACT_CHANGE_WALL:
            current_wall = (current_wall + 1) % NUM_WALLPAPERS;
            notify_post("Wallpaper", "Switched"); break;
        case ACT_TOGGLE_THEME:
            theme_set_dark(!theme()->is_dark);
            notify_post("Theme", theme()->is_dark ? "Dark" : "Light"); break;
        case ACT_SHOW_DESKTOP:
            for (int i = 0; i < win_count; i++) wins[i].minimized = 1;
            break;
        case ACT_OPEN_SETTINGS:
            open_app("Settings"); break;

        case ACT_WIN_MIN:
            if (ctx_window_idx >= 0) wins[ctx_window_idx].minimized = 1; break;
        case ACT_WIN_MAX:
            if (ctx_window_idx >= 0) maximize_window(&wins[ctx_window_idx]); break;
        case ACT_WIN_CLOSE:
            if (ctx_window_idx >= 0) wins[ctx_window_idx].visible = 0; break;
        case ACT_WIN_SEND_DESK1: if (ctx_window_idx >= 0) wins[ctx_window_idx].desktop = 0; break;
        case ACT_WIN_SEND_DESK2: if (ctx_window_idx >= 0) wins[ctx_window_idx].desktop = 1; break;
        case ACT_WIN_SEND_DESK3: if (ctx_window_idx >= 0) wins[ctx_window_idx].desktop = 2; break;
        case ACT_WIN_SEND_DESK4: if (ctx_window_idx >= 0) wins[ctx_window_idx].desktop = 3; break;

        case ACT_DOCK_OPEN:
            if (ctx_dock_idx >= 0) open_app(dock_app_names[ctx_dock_idx]); break;
        case ACT_DOCK_QUIT: {
            int wi = find_app_window(dock_app_names[ctx_dock_idx]);
            if (wi >= 0) wins[wi].visible = 0;
            break;
        }

        case ACT_POWER_SLEEP: notify_post("Sleep", "(simulated)"); break;
        case ACT_POWER_LOCK:  locked = 1; pw_len = 0; pw_wrong = 0; break;
        case ACT_POWER_LOGOUT: auth_logout(); locked = 1; pw_len = 0; pw_wrong = 0; break;
        case ACT_POWER_RESTART:  power_reboot();   break;
        case ACT_POWER_SHUTDOWN: power_shutdown(); break;
    }
}

void gui_run() {
    if (!fb_available()) return;
    if (!comp_init())    return;
    theme_init();
    apps_register_all();
    launcher_set_callback(on_launch);
    ctxmenu_clear(&menu);

    notify_post("Welcome to Vyro OS 2.0", "Right-click anywhere for actions");

    int dragging = -1, resizing = -1;
    int dox = 0, doy = 0;
    uint8_t prev_btn = 0;

    while (1) {
        int last_key = 0;
        if (keyboard_has_input()) {
            char c = keyboard_getchar();
            if (locked) { lockscreen_handle_key(c); }
            else {
                if (c == 0x1B) break;
                if (c == 't' || c == 'T') { theme_set_dark(!theme()->is_dark); }
                // Virtual desktop switching: keys '!', '@', '#', '$' (shift+1..4)
                else if (c == '1' || c == '2' || c == '3' || c == '4') {
                    current_desktop = c - '1';
                }
                else if (c == 'l' || c == 'L') { locked = 1; pw_len = 0; pw_wrong = 0; }
                else last_key = (int)(unsigned char)c;
            }
        }
        if (last_key && win_count > 0)
            wins[win_count - 1].last_key = last_key;

        int mx = mouse_x(), my = mouse_y();
        uint8_t btn = mouse_buttons();
        int lpress = (btn & MOUSE_LEFT)  && !(prev_btn & MOUSE_LEFT);
        int rpress = (btn & MOUSE_RIGHT) && !(prev_btn & MOUSE_RIGHT);
        int rel    = !(btn & MOUSE_LEFT) && (prev_btn & MOUSE_LEFT);

        if (locked) {
            render(mx, my, 0); prev_btn = btn; sleep_ms(16); continue;
        }

        // Menu handling first — close on click outside, dispatch action on item click
        if (menu.visible && lpress) {
            int r = ctxmenu_handle_click(&menu, mx, my);
            if (r > 0) dispatch_action(menu.last_action);
            // swallow this click (don't fall through)
            lpress = 0;
        }

        // Power menu / quick settings click-outside-to-close
        if (lpress && (show_power_menu || show_quick_settings)) {
            int hit = 0;
            if (show_power_menu && point_in(mx, my, comp_width() - 220, TOPBAR_H + 4, 200, 178)) hit = 1;
            if (show_quick_settings && point_in(mx, my, comp_width() - 300, TOPBAR_H + 4, 280, 280)) hit = 1;
            // Power menu items
            if (show_power_menu && hit) {
                int px_ = comp_width() - 220, py_ = TOPBAR_H + 4;
                int item_actions[] = { ACT_POWER_SLEEP, ACT_POWER_LOCK, ACT_POWER_LOGOUT, ACT_POWER_RESTART, ACT_POWER_SHUTDOWN };
                for (int i = 0; i < 5; i++) {
                    int ry = py_ + 6 + i * 32;
                    if (my >= ry && my < ry + 28) { dispatch_action(item_actions[i]); break; }
                }
                show_power_menu = 0; lpress = 0;
            } else if (!hit) {
                show_power_menu = 0; show_quick_settings = 0;
            }
        }

        if (lpress) {
            // Top bar power button (region around "[P]")
            if (point_in(mx, my, comp_width() - 116, 0, 24, TOPBAR_H)) {
                show_power_menu = !show_power_menu; show_quick_settings = 0;
            }
            else if (point_in(mx, my, comp_width() - 172, 0, 24, TOPBAR_H)) {
                show_quick_settings = !show_quick_settings; show_power_menu = 0;
            }
            else if (hovered_dock >= 0) {
                open_app(dock_app_names[hovered_dock]);
            } else {
                for (int i = win_count - 1; i >= 0; i--) {
                    window_t* w = &wins[i];
                    if (!w->visible || w->minimized || w->desktop != current_desktop) continue;
                    if (point_in(mx, my, w->x + 10, w->y + 8, 12, 12)) { w->visible = 0; break; }
                    if (point_in(mx, my, w->x + 28, w->y + 8, 12, 12)) { w->minimized = 1; break; }
                    if (point_in(mx, my, w->x + 46, w->y + 8, 12, 12)) { maximize_window(w); break; }
                    if (!w->maximized && point_in(mx, my, w->x + w->w - RESIZE_GRIP,
                            w->y + w->h - RESIZE_GRIP, RESIZE_GRIP, RESIZE_GRIP)) {
                        bring_to_front(i); resizing = win_count - 1;
                        dox = mx - (w->x + w->w); doy = my - (w->y + w->h); break;
                    }
                    if (point_in(mx, my, w->x, w->y, w->w, TITLE_H)) {
                        bring_to_front(i); dragging = win_count - 1;
                        dox = mx - wins[dragging].x; doy = my - wins[dragging].y; break;
                    }
                    if (point_in(mx, my, w->x, w->y, w->w, w->h)) { bring_to_front(i); break; }
                }
            }
        }

        // Right-click context menus
        if (rpress) {
            ctx_window_idx = -1; ctx_dock_idx = -1;
            if (hovered_dock >= 0) {
                ctx_source = CTX_DOCK; ctx_dock_idx = hovered_dock;
                build_dock_menu(hovered_dock);
                ctxmenu_show(&menu, mx, my);
            } else {
                int hit_window = -1;
                for (int i = win_count - 1; i >= 0; i--) {
                    if (!wins[i].visible || wins[i].minimized || wins[i].desktop != current_desktop) continue;
                    if (point_in(mx, my, wins[i].x, wins[i].y, wins[i].w, wins[i].h)) { hit_window = i; break; }
                }
                if (hit_window >= 0) {
                    ctx_source = CTX_WINDOW; ctx_window_idx = hit_window;
                    build_window_menu();
                    ctxmenu_show(&menu, mx, my);
                } else if (my < (int)comp_height() - DOCK_H - 16 && my > TOPBAR_H) {
                    ctx_source = CTX_DESKTOP;
                    build_desktop_menu();
                    ctxmenu_show(&menu, mx, my);
                }
            }
        }

        if (rel) { dragging = -1; resizing = -1; }
        if (dragging >= 0 && (btn & MOUSE_LEFT)) {
            window_t* w = &wins[dragging];
            int nx = mx - dox, ny = my - doy;
            if (ny < TOPBAR_H) ny = TOPBAR_H;
            w->x = nx; w->y = ny; w->maximized = 0;
        }
        if (resizing >= 0 && (btn & MOUSE_LEFT)) {
            window_t* w = &wins[resizing];
            int nw = mx - dox - w->x; int nh = my - doy - w->y;
            if (nw < 240) nw = 240; if (nh < 160) nh = 160;
            w->w = nw; w->h = nh;
        }

        render(mx, my, lpress);
        prev_btn = btn;
        sleep_ms(16);
    }
}
