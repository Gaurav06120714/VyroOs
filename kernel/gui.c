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

#define TOPBAR_H    32
#define TASKBAR_H   26
#define DOCK_H      76
#define TITLE_H     28
#define MAX_WINS    8
#define DOCK_ITEMS  10
#define RESIZE_GRIP 14
#define NUM_DESKTOPS 4

#define PANEL_W     308

typedef struct {
    const app_def_t* app;
    int x, y, w, h;
    int saved_x, saved_y, saved_w, saved_h;
    uint8_t visible;
    uint8_t minimized;
    uint8_t maximized;
    uint8_t desktop;
    uint64_t opened_at;
    int last_key;
} window_t;

static window_t wins[MAX_WINS];
static int      win_count = 0;
static int      hovered_dock = -1;
static int      current_desktop = 0;

static ctxmenu_t menu;
static int       show_power_menu    = 0;
static int       show_quick_settings = 0;
static int       show_notif_center   = 0;
static int       locked  = 0;
static char      pw_buf[32];
static int       pw_len  = 0;
static int       pw_wrong = 0;

typedef enum { CTX_NONE, CTX_DESKTOP, CTX_WINDOW, CTX_DOCK } ctx_source_t;
static ctx_source_t ctx_source    = CTX_NONE;
static int          ctx_window_idx = -1;
static int          ctx_dock_idx   = -1;

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
    "Files", "Terminal", "Browser", "TextEdit",
    "Calculator", "Calendar", "Notes", "Settings",
    "Task Manager", "App Store"
};
static const icon_t* dock_icons[DOCK_ITEMS] = {
    &ICON_FINDER, &ICON_TERMINAL, &ICON_BROWSER,  &ICON_APPS,
    &ICON_APPS,   &ICON_APPS,    &ICON_APPS,       &ICON_SETTINGS,
    &ICON_APPS,   &ICON_TRASH
};

static uint32_t wallpapers[][2] = {
    { 0x161C2C, 0x0C101C },
    { 0x2A1530, 0x16081A },
    { 0x102822, 0x081410 },
    { 0x301010, 0x180808 },
    { 0xDDE4F0, 0xC0CCDE },
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
        wins[existing].visible   = 1;
        wins[existing].minimized = 0;
        wins[existing].opened_at = timer_ticks();
        wins[existing].desktop   = current_desktop;
        bring_to_front(existing);
        return;
    }
    const app_def_t* a = app_find(name);
    if (!a) return;
    if (win_count >= MAX_WINS) win_count = MAX_WINS - 1;
    window_t* w = &wins[win_count++];
    w->app = a;
    int usable_w = (int)comp_width() - PANEL_W;
    int usable_h = (int)comp_height() - TOPBAR_H - TASKBAR_H - DOCK_H - 16;
    int dw_ = a->default_w; if (dw_ > usable_w - 40) dw_ = usable_w - 40;
    int dh_ = a->default_h + TITLE_H; if (dh_ > usable_h) dh_ = usable_h;
    w->w = dw_; w->h = dh_;
    w->x = 40 + (win_count * 28) % 80;
    w->y = TOPBAR_H + TASKBAR_H + 20 + (win_count * 20) % 60;
    w->visible = 1; w->minimized = 0; w->maximized = 0;
    w->opened_at = timer_ticks();
    w->last_key  = 0;
    w->desktop   = current_desktop;
}
static void maximize_window(window_t* w) {
    if (!w->maximized) {
        w->saved_x = w->x; w->saved_y = w->y;
        w->saved_w = w->w; w->saved_h = w->h;
        w->x = 0; w->y = TOPBAR_H + TASKBAR_H;
        w->w = (int)comp_width() - PANEL_W;
        w->h = (int)comp_height() - TOPBAR_H - TASKBAR_H - DOCK_H - 16;
        w->maximized = 1;
    } else {
        w->x = w->saved_x; w->y = w->saved_y;
        w->w = w->saved_w; w->h = w->saved_h;
        w->maximized = 0;
    }
}

static void draw_btn(int x, int y, int sz, uint32_t bg, char label, uint32_t fg) {
    comp_rect(x, y, sz, sz, bg);
    comp_border(x, y, sz, sz, 0x00000040);
    char s[2] = { label, 0 };
    comp_text(x + sz/2 - 4, y + sz/2 - 7, s, fg, bg);
}

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
    extern int gui_glass_mode(void);
    if (gui_glass_mode() && age >= 8) {
        comp_glass_panel(rx, ry, rw, TITLE_H,
                         focused ? 0xFFFFFF : 0xC0C0C0,
                         focused ? 96 : 70, 0, t->win_border);
        comp_glass_panel(rx, ry + TITLE_H, rw, rh - TITLE_H,
                         t->win_body, 200, 0, t->win_border);
    } else {
        comp_rect(rx, ry, rw, TITLE_H, tcol);
        comp_rect(rx, ry + TITLE_H, rw, rh - TITLE_H, t->win_body);
    }
    comp_border(rx, ry, rw, rh, focused ? t->accent : t->win_border);

    if (age >= 8) {
        int btn_y = w->y + (TITLE_H - 14) / 2;
        draw_btn(w->x + 10, btn_y, 14, t->danger,  'x', 0x800000);
        draw_btn(w->x + 28, btn_y, 14, 0xDBA520,   '-', 0x6B4E00);
        draw_btn(w->x + 46, btn_y, 14, t->success,  '+', 0x006B00);

        int title_len = 0;
        const char* tn = w->app->name;
        while (tn[title_len]) title_len++;
        int title_x = rx + (rw - title_len * 8) / 2;
        if (title_x < rx + 70) title_x = rx + 70;
        comp_text(title_x, w->y + (TITLE_H - 14) / 2, w->app->name, t->text, tcol);

        if (!w->maximized) {
            for (int i = 0; i < 10; i++) {
                comp_pixel(w->x + w->w - 3 - i, w->y + w->h - 3, t->text_dim);
                comp_pixel(w->x + w->w - 3,     w->y + w->h - 3 - i, t->text_dim);
                if (i < 5) {
                    comp_pixel(w->x + w->w - 3 - i, w->y + w->h - 3 - i, t->text_dim);
                }
            }
        }

        int body_x = w->x,       body_y = w->y + TITLE_H;
        int body_w = w->w,        body_h = w->h - TITLE_H;
        int body_mx = mx - body_x, body_my = my - body_y;
        int in_body = (body_mx >= 0 && body_mx < body_w &&
                       body_my >= 0 && body_my < body_h);
        app_ctx_t ctx = {
            .mx = body_mx, .my = body_my, .btn = 0,
            .clicked  = (in_body && focused && clicked) ? 1 : 0,
            .key      = (focused ? w->last_key : 0),
            .width    = body_w, .height = body_h,
            .origin_x = body_x, .origin_y = body_y
        };
        w->app->render(&ctx);
        w->last_key = 0;
    }
}

static void draw_topbar(int mx, int my) {
    const theme_t* t = theme();
    uint32_t bar_bg = t->is_dark ? 0x0F1520 : 0xE8ECF4;
    uint32_t sep    = t->is_dark ? 0x2A3248 : 0xC0C8D8;
    uint32_t txt    = t->text;
    uint32_t dim    = t->text_dim;

    comp_rect(0, 0, (int)comp_width(), TOPBAR_H, bar_bg);

    comp_rect(0, TOPBAR_H - 1, (int)comp_width(), 1, sep);

    comp_text(10, (TOPBAR_H - 14) / 2, "Vyro", t->accent_hi, bar_bg);

    int sep1 = 52;
    comp_rect(sep1, 6, 1, TOPBAR_H - 12, sep);

    const char* menu_items[] = { "File", "Edit", "View", "Window", "Help" };
    int mx_ = sep1 + 10;
    for (int i = 0; i < 5; i++) {
        int len = 0; while (menu_items[i][len]) len++;
        int hover = point_in(mx, my, mx_, 0, len*8 + 12, TOPBAR_H);
        if (hover) comp_rect(mx_, 2, len*8 + 12, TOPBAR_H - 4, t->accent);
        comp_text(mx_ + 6, (TOPBAR_H - 14) / 2, menu_items[i],
                  hover ? 0xFFFFFF : txt, hover ? t->accent : bar_bg);
        mx_ += len*8 + 14;
    }

    int right = (int)comp_width() - PANEL_W - 8;

    right -= 52;
    rtc_time_t rt; rtc_read(&rt);
    char clock[9] = "00:00:00";
    d2(clock,   rt.hour);
    d2(clock+3, rt.minute);
    d2(clock+6, rt.second);
    comp_text(right, (TOPBAR_H - 14) / 2, clock, txt, bar_bg);

    right -= 6;
    comp_rect(right, 6, 1, TOPBAR_H - 12, sep);

    right -= 36;
    int pwr_hover = point_in(mx, my, right, 2, 34, TOPBAR_H - 4);
    if (pwr_hover) comp_rect(right, 2, 34, TOPBAR_H - 4, 0x991111);
    comp_text(right + 4, (TOPBAR_H - 14) / 2, "Pwr",
              pwr_hover ? 0xFFFFFF : 0xFF8888, pwr_hover ? 0x991111 : bar_bg);

    right -= 6;
    comp_rect(right, 6, 1, TOPBAR_H - 12, sep);

    right -= 32;
    int qs_hover = point_in(mx, my, right, 2, 30, TOPBAR_H - 4);
    if (qs_hover) comp_rect(right, 2, 30, TOPBAR_H - 4, t->accent);
    comp_text(right + 3, (TOPBAR_H - 14) / 2, "QS",
              qs_hover ? 0xFFFFFF : dim, qs_hover ? t->accent : bar_bg);

    right -= 6;
    comp_rect(right, 6, 1, TOPBAR_H - 12, sep);

    right -= 32;
    int bell_hover = point_in(mx, my, right, 2, 30, TOPBAR_H - 4);
    if (bell_hover || show_notif_center) comp_rect(right, 2, 30, TOPBAR_H - 4, t->accent);
    comp_text(right + 4, (TOPBAR_H - 14) / 2, "[B]",
              (bell_hover || show_notif_center) ? 0xFFFFFF : dim,
              (bell_hover || show_notif_center) ? t->accent : bar_bg);

    right -= 6;
    comp_rect(right, 6, 1, TOPBAR_H - 12, sep);

    right -= 40;
    comp_text(right, (TOPBAR_H - 14) / 2, "Vol+", dim, bar_bg);

    right -= 6;
    comp_rect(right, 6, 1, TOPBAR_H - 12, sep);

    right -= 48;
    comp_text(right, (TOPBAR_H - 14) / 2, "Wi-Fi", 0x60D0A0, bar_bg);

    right -= 6;
    comp_rect(right, 6, 1, TOPBAR_H - 12, sep);

    right -= 80;
    char uname[32];
    const char* cu = current_user();
    int ul = 0;
    while (cu[ul] && ul < 28) { uname[ul] = cu[ul]; ul++; }
    uname[ul] = 0;
    comp_text(right, (TOPBAR_H - 14) / 2, uname, dim, bar_bg);

    right -= 6;
    comp_rect(right, 6, 1, TOPBAR_H - 12, sep);

    right -= 20;
    char ws[3] = {'W', '0' + (char)(current_desktop + 1), 0};
    comp_text(right, (TOPBAR_H - 14) / 2, ws, t->accent_hi, bar_bg);
}

static void draw_taskbar(int mx, int my, int clicked) {
    const theme_t* t = theme();
    int ty = TOPBAR_H;
    uint32_t bar_bg = t->is_dark ? 0x141A28 : 0xDDE2EE;
    uint32_t sep    = t->is_dark ? 0x252D40 : 0xB8C2D8;

    comp_rect(0, ty, (int)comp_width() - PANEL_W, TASKBAR_H, bar_bg);
    comp_rect(0, ty + TASKBAR_H - 1, (int)comp_width() - PANEL_W, 1, sep);

    int bx = 8;
    for (int i = 0; i < win_count; i++) {
        if (!wins[i].app || wins[i].desktop != current_desktop) continue;
        const char* nm = wins[i].app->name;
        int len = 0; while (nm[len]) len++;
        int bw = len * 8 + 20;
        int focused  = (i == win_count - 1);
        int hover    = point_in(mx, my, bx, ty + 2, bw, TASKBAR_H - 4);
        uint32_t bbg = focused  ? t->accent :
                       hover    ? sep       : bar_bg;
        uint32_t btx = focused  ? 0xFFFFFF  :
                       wins[i].minimized ? t->text_dim : t->text;

        comp_rect(bx, ty + 2, bw, TASKBAR_H - 4, bbg);
        if (focused)
            comp_rect(bx, ty + TASKBAR_H - 3, bw, 2, t->accent_hi);
        comp_text(bx + 10, ty + (TASKBAR_H - 14) / 2, nm, btx, bbg);

        if (clicked && hover) {
            if (wins[i].minimized) {
                wins[i].minimized = 0;
                wins[i].visible   = 1;
                bring_to_front(i);
            } else if (focused) {
                wins[i].minimized = 1;
            } else {
                wins[i].minimized = 0;
                wins[i].visible   = 1;
                bring_to_front(i);
            }
        }
        bx += bw + 4;
    }
}

static void draw_dock(int mx, int my) {
    const theme_t* t = theme();
    int icon_sz = 44, gap = 10, pad = 12;
    int dock_w  = DOCK_ITEMS * icon_sz + (DOCK_ITEMS - 1) * gap + pad * 2;
    int usable_w = (int)comp_width() - PANEL_W;
    int dock_x  = (usable_w - dock_w) / 2;
    int dock_y  = (int)comp_height() - DOCK_H - 6;

    comp_shadow(dock_x - 4, dock_y - 2, dock_w + 8, DOCK_H + 8, t->win_shadow);

    uint32_t dock_bg  = t->is_dark ? 0x1A2030 : 0xDDE3F0;
    uint32_t dock_bdr = t->is_dark ? 0x2E3A52 : 0xA8B4CC;
    comp_rect(dock_x, dock_y, dock_w, DOCK_H, dock_bg);
    comp_border(dock_x, dock_y, dock_w, DOCK_H, dock_bdr);

    comp_rect(dock_x + pad + 4 * (icon_sz + gap) - 2, dock_y + 6,
              1, DOCK_H - 12, dock_bdr);

    hovered_dock = -1;
    for (int i = 0; i < DOCK_ITEMS; i++) {
        int ix     = dock_x + pad + i * (icon_sz + gap);
        int iy     = dock_y + (DOCK_H - icon_sz) / 2 - 4;
        int hover  = point_in(mx, my, ix - 2, dock_y, icon_sz + 4, DOCK_H);
        if (hover) hovered_dock = i;

        uint32_t ibg = hover ? t->accent :
                       (t->is_dark ? 0x232C40 : 0xC8D0E4);
        comp_rect(ix, iy, icon_sz, icon_sz, ibg);
        comp_border(ix, iy, icon_sz, icon_sz, dock_bdr);
        icon_draw(dock_icons[i], ix + 6, iy + 6, 32);

        int wi = find_app_window(dock_app_names[i]);
        if (wi >= 0 && wins[wi].visible && !wins[wi].minimized) {
            int dot_x = ix + icon_sz/2 - 3;
            int dot_y = dock_y + DOCK_H - 7;
            comp_rect(dot_x, dot_y, 6, 3, t->accent_hi);
        }

        const char* nm = dock_app_names[i];
        int len = 0; while (nm[len]) len++;
        int lx  = ix + icon_sz/2 - (len * 8) / 2;
        uint32_t lbg = t->is_dark ? 0x1A2030 : 0xDDE3F0;
        comp_text(lx, dock_y + DOCK_H - 16, nm, hover ? t->accent_hi : t->text_dim, lbg);

        if (hover) {
            int bw = len*8 + 16, bh = 22;
            int bx = ix + icon_sz/2 - bw/2;
            int by = iy - bh - 6;
            comp_shadow(bx, by, bw, bh, t->win_shadow);
            comp_rect(bx, by, bw, bh, t->win_body);
            comp_border(bx, by, bw, bh, t->win_border);
            comp_text(bx + 8, by + 4, nm, t->text, t->win_body);
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
        int nw = 280, nh = 56;
        int nx = (int)comp_width() - PANEL_W - nw - 12;
        int ny = TOPBAR_H + TASKBAR_H + 8 + row * (nh + 8);
        comp_shadow(nx, ny, nw, nh, t->win_shadow);
        comp_glass_panel(nx, ny, nw, nh, 0x000000, 140, 10, t->win_border);
        comp_rect(nx, ny, 4, nh, t->accent);
        comp_text(nx + 12, ny + 8,  arr[i].title, t->text,     0x000000);
        comp_text(nx + 12, ny + 26, arr[i].body,  t->text_dim, 0x000000);
        row++;
    }
}

extern void wallpaper_render(void);
extern void widgets_desktop_render(void);
static void draw_desktop_bg() {
    wallpaper_render();
    widgets_desktop_render();

    int cx = ((int)comp_width() - PANEL_W) / 2;
    int cy = (int)comp_height() - DOCK_H - 28;
    for (int i = 0; i < NUM_DESKTOPS; i++) {
        int dx = cx - (NUM_DESKTOPS * 14) / 2 + i * 14;
        uint32_t c = (i == current_desktop) ? theme()->accent_hi : theme()->win_border;
        comp_rect(dx, cy, 8, 4, c);
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
            if      (c == 'X') comp_pixel(x + i, y + j, 0x000000);
            else if (c == 'w') comp_pixel(x + i, y + j, 0xFFFFFF);
        }
}

static void draw_lockscreen(int mx, int my) {
    (void)mx; (void)my;
    int sw = (int)comp_width(), sh = (int)comp_height();
    comp_gradient_v(0, 0, sw, sh, 0x060C18, 0x000810);
    /* subtle star-like dots */
    for (int i = 0; i < 80; i++) {
        int sx2 = (i * 131 + 47) % sw;
        int sy2 = (i * 97  + 23) % (sh / 2);
        comp_pixel(sx2, sy2, 0x6080C0);
    }
    const theme_t* t = theme();

    rtc_time_t rt; rtc_read(&rt);

    /* --- big clock --- */
    char clk[6]; d2(clk, rt.hour); clk[2] = ':'; d2(clk+3, rt.minute); clk[5] = 0;
    int scale = 5; /* 5x glyph scale */
    int cw2 = 5 * 8 * scale; /* 5 chars * 8px * scale */
    int clk_x = sw/2 - cw2/2;
    int clk_y = 70;
    for (int ci = 0; ci < 5; ci++) {
        for (int dy = 0; dy < scale; dy++)
            for (int dx = 0; dx < scale; dx++)
                comp_glyph(clk_x + ci * 8 * scale + dx, clk_y + dy * 14, clk[ci], 0xEEF4FF, 0);
    }

    /* date line */
    static const char* wday_names[] = {"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};
    static const char* mon_names[]  = {"","January","February","March","April","May","June",
                                       "July","August","September","October","November","December"};
    int wday = 0;
    { int y=rt.year,m=rt.month,d=rt.day;
      if(m<3){m+=12;y--;}
      int K=y%100,J=y/100;
      int h=(d+(13*(m+1))/5+K+K/4+J/4+5*J)%7;
      wday=(h+6)%7; }
    char date_str[40]; int dsp = 0;
    const char* wn = wday_names[wday];
    for(int i=0;wn[i];i++) date_str[dsp++]=wn[i];
    date_str[dsp++]=','; date_str[dsp++]=' ';
    const char* mn = mon_names[rt.month>=1&&rt.month<=12?rt.month:1];
    for(int i=0;mn[i];i++) date_str[dsp++]=mn[i];
    date_str[dsp++]=' ';
    if(rt.day>=10){ date_str[dsp++]='0'+rt.day/10; }
    date_str[dsp++]='0'+rt.day%10;
    date_str[dsp]=0;
    int dl = dsp;
    comp_text(sw/2 - dl*4, clk_y + scale*14 + 8, date_str, 0x8AA0C0, 0);

    /* --- avatar circle --- */
    int av_y = clk_y + scale*14 + 50;
    int av_r = 36;
    int av_cx = sw/2, av_cy = av_y + av_r;
    for (int dy = -av_r; dy <= av_r; dy++) {
        for (int dx = -av_r; dx <= av_r; dx++) {
            if (dx*dx + dy*dy <= av_r*av_r)
                comp_pixel(av_cx + dx, av_cy + dy, 0x2A4080);
        }
    }
    /* ring */
    for (int dy = -av_r; dy <= av_r; dy++) {
        for (int dx = -av_r; dx <= av_r; dx++) {
            int r2 = dx*dx + dy*dy;
            if (r2 > (av_r-2)*(av_r-2) && r2 <= av_r*av_r)
                comp_pixel(av_cx + dx, av_cy + dy, 0x5080C0);
        }
    }
    /* initials */
    const char* uname = current_user();
    char init[2] = { uname[0] >= 'a' && uname[0] <= 'z' ? uname[0] - 32 : uname[0], 0 };
    comp_text(av_cx - 4, av_cy - 7, init, 0xCCDDFF, 0x2A4080);
    comp_text(av_cx - 3, av_cy - 7, init, 0xCCDDFF, 0x2A4080);

    /* username label */
    int ul = 0; while(uname[ul]) ul++;
    comp_text(sw/2 - ul*4, av_cy + av_r + 8, uname, 0xCCDDFF, 0);

    /* --- glass login panel --- */
    int pw = 380, ph = 170;
    int px = (sw - pw) / 2;
    int py = av_cy + av_r + 32;
    comp_glass_panel(px, py, pw, ph, 0x080F1C, 230, 14, 0x304060);

    comp_rect(px, py + 36, pw, 1, 0x304060);
    comp_text(px + 20, py + 42, "Password:", t->text_dim, 0x080F1C);
    comp_rect(px + 20, py + 62, pw - 40, 32, 0x050A14);
    comp_border(px + 20, py + 62, pw - 40, 32, pw_wrong ? 0xFF5050 : 0x4A5870);
    for (int i = 0; i < pw_len && i < 28; i++)
        comp_rect(px + 28 + i*11, py + 74, 7, 8, 0xE0EEFF);

    if (pw_wrong)
        comp_text(px + 20, py + 106, "Wrong password. Try again.", 0xFF7070, 0x080F1C);
    else
        comp_text(px + 20, py + 106, "Press Enter to unlock  |  default: guest", t->text_dim, 0x080F1C);

    comp_rect(px + pw - 110, py + 130, 90, 28, t->accent);
    comp_border(px + pw - 110, py + 130, 90, 28, t->accent_hi);
    comp_text(px + pw - 82, py + 137, "Unlock", 0xFFFFFF, t->accent);

    draw_cursor(mx, my);
    comp_present();
}

static void draw_power_menu(int mx, int my) {
    if (!show_power_menu) return;
    const theme_t* t = theme();
    int pw = 210, ph = 190;
    int px = (int)comp_width() - PANEL_W - pw - 8;
    int py = TOPBAR_H + 4;
    comp_shadow(px, py, pw, ph, t->win_shadow);
    comp_glass_panel(px, py, pw, ph, 0x101828, 230, 8, t->win_border);
    comp_text(px + 14, py + 10, "System", t->accent_hi, 0x101828);
    comp_rect(px, py + 30, pw, 1, t->win_border);

    const char* items[]   = { "Sleep", "Lock Screen", "Log Out", "Restart", "Shut Down" };
    int         actions[] = { ACT_POWER_SLEEP, ACT_POWER_LOCK,
                              ACT_POWER_LOGOUT, ACT_POWER_RESTART, ACT_POWER_SHUTDOWN };
    for (int i = 0; i < 5; i++) {
        int ry = py + 34 + i * 30;
        int hover = point_in(mx, my, px + 2, ry, pw - 4, 26);
        if (hover) comp_rect(px + 2, ry, pw - 4, 26, t->accent);
        uint32_t col = (i >= 3) ? 0xFF8888 : t->text;
        if (hover) col = 0xFFFFFF;
        comp_text(px + 14, ry + 6, items[i], col, hover ? t->accent : 0x101828);
        (void)actions;
    }
}

static void draw_notif_center(int mx, int my) {
    if (!show_notif_center) return;
    (void)mx; (void)my;
    const theme_t* t = theme();
    int pw = 320, ph = (int)comp_height() - TOPBAR_H - 8;
    int px = (int)comp_width() - pw - 4;
    int py = TOPBAR_H + 4;

    comp_shadow(px - 4, py, pw + 8, ph, t->win_shadow);
    comp_glass_panel(px, py, pw, ph, 0x080F1C, 245, 0, t->win_border);

    /* header */
    comp_text(px + 14, py + 10, "Notification Center", t->accent_hi, 0x080F1C);
    comp_rect(px, py + 30, pw, 1, t->win_border);

    notification_t* hist; int count = 0;
    notify_history(&hist, &count);

    if (count == 0) {
        w_label_dim(px + pw/2 - 60, py + ph/2 - 8, "No notifications");
        return;
    }

    int ry = py + 38;
    int shown = 0;
    for (int i = count - 1; i >= 0 && shown < 12; i--) {
        if (!hist[i].title[0]) continue;
        int card_h = 60;
        if (ry + card_h > py + ph - 8) break;

        uint32_t cbg = t->is_dark ? 0x111828 : 0xF0F4FF;
        comp_rect(px + 8, ry, pw - 16, card_h, cbg);
        comp_border(px + 8, ry, pw - 16, card_h, t->win_border);
        /* accent stripe */
        comp_rect(px + 8, ry, 3, card_h, t->accent);
        /* title */
        comp_text(px + 18, ry + 8, hist[i].title, t->text, cbg);
        /* body */
        comp_text(px + 18, ry + 26, hist[i].body, t->text_dim, cbg);
        /* time stamp placeholder */
        comp_text(px + pw - 56, ry + 8, "now", t->text_dim, cbg);

        ry += card_h + 6;
        shown++;
    }

    /* clear all button */
    int btn_y = py + ph - 36;
    comp_rect(px, btn_y - 1, pw, 1, t->win_border);
    int cl = 0; (void)cl;
    comp_rect(px + 8, btn_y + 6, pw - 16, 24, t->is_dark ? 0x1C2230 : 0xDDE3F0);
    comp_border(px + 8, btn_y + 6, pw - 16, 24, t->win_border);
    comp_text(px + pw/2 - 36, btn_y + 12, "Clear All", t->text_dim,
              t->is_dark ? 0x1C2230 : 0xDDE3F0);
}

static int qs_volume     = 60;
static int qs_brightness = 80;
static int qs_wifi       = 1;
static int qs_bt         = 0;
static int qs_dnd        = 0;
static int qs_dark       = 1;

static void draw_quick_settings(int mx, int my) {
    if (!show_quick_settings) return;
    const theme_t* t = theme();
    int pw = 300, ph = 380;
    int px = (int)comp_width() - pw - 8;
    int py = TOPBAR_H + 4;
    comp_shadow(px, py, pw, ph, t->win_shadow);
    comp_glass_panel(px, py, pw, ph, 0x0C1220, 245, 12, t->win_border);

    /* header */
    comp_text(px + 14, py + 10, "Quick Settings", t->accent_hi, 0x0C1220);
    comp_rect(px, py + 30, pw, 1, t->win_border);

    int ry = py + 38;

    /* toggle rows */
    struct { const char* label; const char* sub; int* state; } rows[4];
    rows[0].label = "Wi-Fi";          rows[0].sub = qs_wifi ? "Connected" : "Off";  rows[0].state = &qs_wifi;
    rows[1].label = "Bluetooth";      rows[1].sub = qs_bt   ? "On"        : "Off";  rows[1].state = &qs_bt;
    rows[2].label = "Do Not Disturb"; rows[2].sub = qs_dnd  ? "Enabled"   : "Off";  rows[2].state = &qs_dnd;
    rows[3].label = "Dark Mode";      rows[3].sub = qs_dark ? "On"        : "Off";  rows[3].state = &qs_dark;

    for (int i = 0; i < 4; i++) {
        comp_text(px + 14, ry + 4, rows[i].label, t->text,     0x0C1220);
        comp_text(px + 14, ry + 20, rows[i].sub,  t->text_dim, 0x0C1220);
        int ns = w_toggle(px + pw - 60, ry + 4, *rows[i].state, mx, my, 1);
        if (ns != *rows[i].state) {
            *rows[i].state = ns;
            if (i == 3) theme_set_dark(ns);
        }
        ry += 42;
    }

    comp_rect(px, ry, pw, 1, t->win_border); ry += 12;

    /* brightness */
    comp_text(px + 14, ry, "Brightness", t->text_dim, 0x0C1220);
    char bpct[8]; int bv = qs_brightness, bp = 0;
    if (!bv) { bpct[0]='0'; bpct[1]='%'; bpct[2]=0; }
    else { char r[4]; int n=0; while(bv){r[n++]='0'+bv%10;bv/=10;} while(n) bpct[bp++]=r[--n]; bpct[bp++]='%'; bpct[bp]=0; }
    comp_text(px + pw - 36, ry, bpct, t->accent_hi, 0x0C1220);
    ry += 18;
    w_progress(px + 14, ry, pw - 72, 14, qs_brightness);
    if (w_button(px + pw - 52, ry - 2, 22, 18, "-", mx, my, 1) && qs_brightness > 0)   qs_brightness -= 5;
    if (w_button(px + pw - 26, ry - 2, 22, 18, "+", mx, my, 1) && qs_brightness < 100) qs_brightness += 5;
    ry += 26;

    /* volume */
    comp_text(px + 14, ry, "Volume", t->text_dim, 0x0C1220);
    char vpct[8]; int vv = qs_volume, vp = 0;
    if (!vv) { vpct[0]='0'; vpct[1]='%'; vpct[2]=0; }
    else { char r2[4]; int n2=0; while(vv){r2[n2++]='0'+vv%10;vv/=10;} while(n2) vpct[vp++]=r2[--n2]; vpct[vp++]='%'; vpct[vp]=0; }
    comp_text(px + pw - 36, ry, vpct, t->accent_hi, 0x0C1220);
    ry += 18;
    w_progress(px + 14, ry, pw - 72, 14, qs_volume);
    if (w_button(px + pw - 52, ry - 2, 22, 18, "-", mx, my, 1) && qs_volume > 0)   qs_volume -= 5;
    if (w_button(px + pw - 26, ry - 2, 22, 18, "+", mx, my, 1) && qs_volume < 100) qs_volume += 5;
    ry += 26;

    comp_rect(px, ry, pw, 1, t->win_border); ry += 10;

    /* shortcut tiles */
    const char* tiles[] = { "Settings", "Network", "Display", "Sound" };
    int tile_w = (pw - 28) / 2;
    for (int i = 0; i < 4; i++) {
        int tx = px + 14 + (i % 2) * (tile_w + 4);
        int ty2 = ry + (i / 2) * 36;
        uint32_t tile_bg = t->is_dark ? 0x1C2640 : 0xCDD4E8;
        int thov = (mx >= tx && mx < tx + tile_w && my >= ty2 && my < ty2 + 30);
        if (thov) tile_bg = t->accent;
        comp_rect(tx, ty2, tile_w, 30, tile_bg);
        comp_border(tx, ty2, tile_w, 30, t->win_border);
        int tl = 0; while(tiles[i][tl]) tl++;
        comp_text(tx + tile_w/2 - tl*4, ty2 + 8, tiles[i],
                  thov ? 0xFFFFFF : t->text, tile_bg);
    }
}

static void render(int mx, int my, int clicked) {
    if (locked) { draw_lockscreen(mx, my); return; }

    draw_desktop_bg();
    for (int i = 0; i < win_count; i++)
        draw_window(&wins[i], i == win_count - 1, mx, my, clicked);
    draw_topbar(mx, my);
    draw_taskbar(mx, my, clicked);
    widgets_panel_draw((int)comp_width() - PANEL_W, TOPBAR_H + 4, PANEL_W);
    draw_dock(mx, my);
    draw_notifications();
    draw_power_menu(mx, my);
    draw_quick_settings(mx, my);
    draw_notif_center(mx, my);
    ctxmenu_draw(&menu, mx, my);
    draw_cursor(mx, my);
    comp_present();
}

extern void launcher_set_callback(void (*cb)(const char*));
static void on_launch(const char* name) { open_app(name); }

static void lockscreen_handle_key(char c) {
    if (c == '\n') {
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

static void build_desktop_menu() {
    ctxmenu_clear(&menu);
    ctxmenu_add(&menu, "New Folder",        ACT_NEW_FOLDER,    CTX_ITEM_NORMAL);
    ctxmenu_add(&menu, "",                  0,                 CTX_ITEM_SEP);
    ctxmenu_add(&menu, "Change Wallpaper",  ACT_CHANGE_WALL,   CTX_ITEM_NORMAL);
    ctxmenu_add(&menu, "Toggle Theme",      ACT_TOGGLE_THEME,  CTX_ITEM_NORMAL);
    ctxmenu_add(&menu, "",                  0,                 CTX_ITEM_SEP);
    ctxmenu_add(&menu, "Show Desktop",      ACT_SHOW_DESKTOP,  CTX_ITEM_NORMAL);
    ctxmenu_add(&menu, "Open Settings",     ACT_OPEN_SETTINGS, CTX_ITEM_NORMAL);
}
static void build_window_menu() {
    ctxmenu_clear(&menu);
    ctxmenu_add(&menu, "Minimize",           ACT_WIN_MIN,        CTX_ITEM_NORMAL);
    ctxmenu_add(&menu, "Toggle Maximize",    ACT_WIN_MAX,        CTX_ITEM_NORMAL);
    ctxmenu_add(&menu, "",                   0,                  CTX_ITEM_SEP);
    ctxmenu_add(&menu, "Send to Desktop 1",  ACT_WIN_SEND_DESK1, CTX_ITEM_NORMAL);
    ctxmenu_add(&menu, "Send to Desktop 2",  ACT_WIN_SEND_DESK2, CTX_ITEM_NORMAL);
    ctxmenu_add(&menu, "Send to Desktop 3",  ACT_WIN_SEND_DESK3, CTX_ITEM_NORMAL);
    ctxmenu_add(&menu, "Send to Desktop 4",  ACT_WIN_SEND_DESK4, CTX_ITEM_NORMAL);
    ctxmenu_add(&menu, "",                   0,                  CTX_ITEM_SEP);
    ctxmenu_add(&menu, "Close",              ACT_WIN_CLOSE,      CTX_ITEM_DANGER);
}
static void build_dock_menu(int idx) {
    ctxmenu_clear(&menu);
    ctxmenu_add(&menu, "Open",              ACT_DOCK_OPEN, CTX_ITEM_NORMAL);
    ctxmenu_add(&menu, "Pin to Dock",       ACT_DOCK_PIN,  CTX_ITEM_DISABLED);
    ctxmenu_add(&menu, "",                  0,             CTX_ITEM_SEP);
    int wi = find_app_window(dock_app_names[idx]);
    if (wi >= 0 && wins[wi].visible)
        ctxmenu_add(&menu, "Quit App",      ACT_DOCK_QUIT, CTX_ITEM_DANGER);
}

static void dispatch_action(int act) {
    switch (act) {
        case ACT_NEW_FOLDER:
            notify_post("New Folder", "Created"); break;
        case ACT_CHANGE_WALL:
            current_wall = (current_wall + 1) % NUM_WALLPAPERS;
            notify_post("Wallpaper", "Switched"); break;
        case ACT_TOGGLE_THEME:
            theme_set_dark(!theme()->is_dark);
            notify_post("Theme", theme()->is_dark ? "Dark" : "Light"); break;
        case ACT_SHOW_DESKTOP:
            for (int i = 0; i < win_count; i++) wins[i].minimized = 1; break;
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

        case ACT_POWER_SLEEP:  notify_post("Sleep", "(simulated)"); break;
        case ACT_POWER_LOCK:   locked = 1; pw_len = 0; pw_wrong = 0; break;
        case ACT_POWER_LOGOUT: auth_logout(); locked = 1; pw_len = 0; pw_wrong = 0; break;
        case ACT_POWER_RESTART:  power_reboot();   break;
        case ACT_POWER_SHUTDOWN: power_shutdown(); break;
    }
}

static int glass_mode_on = 0;
void gui_set_glass_mode(int on) { glass_mode_on = on ? 1 : 0; }
int  gui_glass_mode(void)       { return glass_mode_on; }

void gui_run() {
    if (!fb_available()) return;
    if (!comp_init())    return;
    theme_init();
    apps_register_all();
    launcher_set_callback(on_launch);
    ctxmenu_clear(&menu);

    comp_revalidate();
    notify_post("Welcome to Vyro OS 2.0", "Right-click desktop for options");
    open_app("Files");

    int dragging = -1, resizing = -1;
    int dox = 0, doy = 0;
    uint8_t prev_btn = 0;

    while (1) {
        comp_revalidate();
        if (!comp_canary_ok()) comp_canary_dump();
        mouse_poll();

        int last_key = 0;
        if (keyboard_has_input()) {
            char c = keyboard_getchar();
            if (locked) {
                lockscreen_handle_key(c);
            } else {
                if (c == 0x1B) break;
                if (c == 't' || c == 'T') { theme_set_dark(!theme()->is_dark); }
                else if (c == '1' || c == '2' || c == '3' || c == '4') {
                    current_desktop = c - '1';
                }
                else if (c == 'l' || c == 'L') { locked = 1; pw_len = 0; pw_wrong = 0; }
                else last_key = (int)(unsigned char)c;
            }
        }
        if (last_key && win_count > 0)
            wins[win_count - 1].last_key = last_key;

        int mx     = mouse_x(), my = mouse_y();
        uint8_t btn = mouse_buttons();
        int lpress = (btn & MOUSE_LEFT)  && !(prev_btn & MOUSE_LEFT);
        int rpress = (btn & MOUSE_RIGHT) && !(prev_btn & MOUSE_RIGHT);
        int rel    = !(btn & MOUSE_LEFT) && (prev_btn & MOUSE_LEFT);

        if (locked) {
            render(mx, my, 0); prev_btn = btn; sleep_ms(16); continue;
        }

        if (menu.visible && lpress) {
            int r = ctxmenu_handle_click(&menu, mx, my);
            if (r > 0) dispatch_action(menu.last_action);
            lpress = 0;
        }

        if (lpress && (show_power_menu || show_quick_settings || show_notif_center)) {
            int pw_x  = (int)comp_width() - PANEL_W - 210 - 8;
            int qs_x  = (int)comp_width() - pw_x;   /* unused but keep for ref */
            int nc_x  = (int)comp_width() - 320 - 4;
            int hit = 0;
            if (show_power_menu     && point_in(mx, my, pw_x, TOPBAR_H + 4, 210, 190)) hit = 1;
            if (show_quick_settings && point_in(mx, my, (int)comp_width() - 300 - 8, TOPBAR_H + 4, 300, 380)) hit = 1;
            if (show_notif_center   && point_in(mx, my, nc_x, TOPBAR_H + 4, 320, (int)comp_height() - TOPBAR_H - 8)) hit = 1;

            if (show_power_menu && hit) {
                int py_ = TOPBAR_H + 4; (void)pw_x;
                int item_acts[] = { ACT_POWER_SLEEP, ACT_POWER_LOCK, ACT_POWER_LOGOUT,
                                    ACT_POWER_RESTART, ACT_POWER_SHUTDOWN };
                for (int i = 0; i < 5; i++) {
                    int ry = py_ + 34 + i * 30;
                    if (my >= ry && my < ry + 26) { dispatch_action(item_acts[i]); break; }
                }
                show_power_menu = 0; lpress = 0;
            } else if (!hit) {
                show_power_menu = 0; show_quick_settings = 0; show_notif_center = 0;
            }
            (void)qs_x;
        }

        if (lpress) {
            int usable_w  = (int)comp_width() - PANEL_W;
            int pwr_right = usable_w - 8;
            int pwr_x     = pwr_right - 36;
            int qs_x      = pwr_x - 6 - 32;
            int bell_x    = qs_x - 6 - 32;

            if (point_in(mx, my, pwr_x, 2, 34, TOPBAR_H - 4)) {
                show_power_menu     = !show_power_menu;
                show_quick_settings = 0; show_notif_center = 0;
            } else if (point_in(mx, my, qs_x, 2, 30, TOPBAR_H - 4)) {
                show_quick_settings = !show_quick_settings;
                show_power_menu = 0; show_notif_center = 0;
            } else if (point_in(mx, my, bell_x, 2, 30, TOPBAR_H - 4)) {
                show_notif_center   = !show_notif_center;
                show_power_menu = 0; show_quick_settings = 0;
            } else if (point_in(mx, my, 0, TOPBAR_H, usable_w, TASKBAR_H)) {
            } else if (hovered_dock >= 0) {
                open_app(dock_app_names[hovered_dock]);
            } else {
                for (int i = win_count - 1; i >= 0; i--) {
                    window_t* w = &wins[i];
                    if (!w->visible || w->minimized || w->desktop != current_desktop) continue;
                    int btn_y = w->y + (TITLE_H - 14) / 2;
                    if (point_in(mx, my, w->x + 10, btn_y, 14, 14)) { w->visible = 0; break; }
                    if (point_in(mx, my, w->x + 28, btn_y, 14, 14)) { w->minimized = 1; break; }
                    if (point_in(mx, my, w->x + 46, btn_y, 14, 14)) { maximize_window(w); break; }
                    if (!w->maximized &&
                        point_in(mx, my, w->x + w->w - RESIZE_GRIP,
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

        if (rpress) {
            ctx_window_idx = -1; ctx_dock_idx = -1;
            if (hovered_dock >= 0) {
                ctx_source = CTX_DOCK; ctx_dock_idx = hovered_dock;
                build_dock_menu(hovered_dock);
                ctxmenu_show(&menu, mx, my);
            } else {
                int hit_window = -1;
                for (int i = win_count - 1; i >= 0; i--) {
                    if (!wins[i].visible || wins[i].minimized ||
                        wins[i].desktop != current_desktop) continue;
                    if (point_in(mx, my, wins[i].x, wins[i].y, wins[i].w, wins[i].h)) {
                        hit_window = i; break;
                    }
                }
                if (hit_window >= 0) {
                    ctx_source = CTX_WINDOW; ctx_window_idx = hit_window;
                    build_window_menu();
                    ctxmenu_show(&menu, mx, my);
                } else if (my > TOPBAR_H + TASKBAR_H &&
                           my < (int)comp_height() - DOCK_H - 16) {
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
            if (ny < TOPBAR_H + TASKBAR_H) ny = TOPBAR_H + TASKBAR_H;
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
