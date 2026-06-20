#include "apps.h"
#include "../widgets.h"
#include "../theme.h"
#include "../compositor.h"
#include "../notify.h"
#include "../security.h"
#include "../net.h"
#include "../smp.h"

static int sel_section  = 0;
static int brightness   = 80;
static int volume       = 60;
static int wifi_on      = 1;
static int bt_on        = 0;
static int notif_on     = 1;
static int animations   = 1;
static const char* sections[] = {
    "General", "Display", "Personalization", "Accounts",
    "Network", "Security", "Storage", "About"
};
#define N_SECTIONS 8

static void render_general(app_ctx_t* c) {
    int x = c->origin_x + 200;
    int y = c->origin_y + 16;
    int amx = c->mx + c->origin_x, amy = c->my + c->origin_y;
    const theme_t* t = theme();
    w_label_color(x, y, "General", t->accent_hi); y += 32;

    w_label(x, y, "Dark Mode");
    int dark = t->is_dark;
    int nd = w_toggle(x + 200, y - 4, dark, amx, amy, c->clicked);
    if (nd != dark) { theme_set_dark(nd); notify_post("Theme", nd?"Dark":"Light"); }
    w_label_dim(x, y + 18, dark ? "Using dark theme" : "Using light theme");
    y += 48; w_separator(x, y, 360); y += 16;

    w_label(x, y, "Animations");
    animations = w_toggle(x + 200, y - 4, animations, amx, amy, c->clicked);
    w_label_dim(x, y + 18, "Window open/close effects");
    y += 48; w_separator(x, y, 360); y += 16;

    w_label(x, y, "Notifications");
    notif_on = w_toggle(x + 200, y - 4, notif_on, amx, amy, c->clicked);
    w_label_dim(x, y + 18, notif_on ? "Notifications enabled" : "Silent mode");
    y += 48; w_separator(x, y, 360); y += 16;

    w_label(x, y, "Volume"); y += 24;
    w_progress(x, y, 280, 14, volume);
    if (w_button(x + 288, y - 2, 28, 18, "+", amx, amy, c->clicked) && volume < 100) volume += 5;
    if (w_button(x + 320, y - 2, 28, 18, "-", amx, amy, c->clicked) && volume > 0)   volume -= 5;
    y += 28; w_separator(x, y, 360); y += 16;

    w_label(x, y, "Brightness"); y += 24;
    w_progress(x, y, 280, 14, brightness);
    if (w_button(x + 288, y - 2, 28, 18, "+", amx, amy, c->clicked) && brightness < 100) brightness += 5;
    if (w_button(x + 320, y - 2, 28, 18, "-", amx, amy, c->clicked) && brightness > 0)   brightness -= 5;
}
static void render_display(app_ctx_t* c) {
    int x = c->origin_x + 200, y = c->origin_y + 16;
    const theme_t* t = theme();
    w_label_color(x, y, "Display", t->accent_hi); y += 32;
    w_label(x, y, "Resolution");      w_label_color(x+180, y, "1024 x 768", t->accent_hi); y += 26;
    w_label(x, y, "Color depth");     w_label_color(x+180, y, "24 bpp BGR",  t->accent_hi); y += 26;
    w_label(x, y, "Framebuffer");     w_label_color(x+180, y, "VESA 0x118",  t->accent_hi); y += 26;
    w_label(x, y, "Refresh rate");    w_label_color(x+180, y, "~60 Hz",      t->accent_hi); y += 26;
    w_label(x, y, "Pixel format");    w_label_color(x+180, y, "BGR24 linear",t->accent_hi); y += 26;
    w_separator(x, y, 360); y += 16;
    w_label(x, y, "Back-buffer");     w_label_color(x+180, y, "0x1000000 (pinned)", t->accent_hi); y += 26;
    w_label(x, y, "Compositor");      w_label_color(x+180, y, "double-buffered",    t->accent_hi); y += 26;
    w_label(x, y, "Glass effects");   w_label_color(x+180, y, "box-blur + tint",    t->accent_hi);
}
static void render_personal(app_ctx_t* c) {
    int x = c->origin_x + 200, y = c->origin_y + 16;
    w_label_color(x, y, "Personalization", theme()->accent_hi); y += 28;
    w_label(x, y, "Wallpaper: gradient (theme)"); y += 20;
    w_label(x, y, "Dock position: bottom-center"); y += 20;
    w_label(x, y, "Top bar: enabled");
}
static void render_accounts(app_ctx_t* c) {
    int x = c->origin_x + 200, y = c->origin_y + 16;
    w_label_color(x, y, "Accounts", theme()->accent_hi); y += 28;
    w_label(x, y, "Current user:"); w_label_color(x + 120, y, current_user(), theme()->accent_hi); y += 20;
    w_label_dim(x, y, current_is_admin() ? "Administrator" : "Standard user"); y += 28;
    user_t* u; int n = user_list(&u);
    for (int i = 0; i < n; i++) {
        w_label(x, y, u[i].name);
        w_label_dim(x + 100, y, u[i].is_admin ? "admin" : "user");
        y += 18;
    }
}
static void render_network(app_ctx_t* c) {
    int x = c->origin_x + 200, y = c->origin_y + 16;
    int amx = c->mx + c->origin_x, amy = c->my + c->origin_y;
    const theme_t* t = theme();
    w_label_color(x, y, "Network", t->accent_hi); y += 32;

    w_label(x, y, "Wi-Fi");
    wifi_on = w_toggle(x + 200, y - 4, wifi_on, amx, amy, c->clicked);
    w_label_dim(x, y + 18, wifi_on ? "Connected via RTL8139" : "Disabled");
    y += 48; w_separator(x, y, 360); y += 16;

    w_label(x, y, "Bluetooth");
    bt_on = w_toggle(x + 200, y - 4, bt_on, amx, amy, c->clicked);
    w_label_dim(x, y + 18, bt_on ? "On" : "Off (no adapter)");
    y += 48; w_separator(x, y, 360); y += 16;

    const uint8_t* mac = net_mac(); const uint8_t* ip = net_ip();
    char buf[64];
    int p = 0;
    const char* h = "0123456789ABCDEF";
    for (int i = 0; i < 6; i++) {
        buf[p++] = h[mac[i] >> 4]; buf[p++] = h[mac[i] & 0xF];
        if (i < 5) buf[p++] = ':';
    } buf[p] = 0;
    w_label(x, y, "MAC:"); w_label_dim(x + 40, y, buf); y += 20;
    p = 0;
    for (int i = 0; i < 4; i++) {
        int v = ip[i]; char tmp[4]; int n2 = 0;
        if (v == 0) tmp[n2++] = '0';
        else { char rev[4]; int r = 0; while (v) { rev[r++] = '0' + (v%10); v/=10; }
               while (r) tmp[n2++] = rev[--r]; }
        for (int j = 0; j < n2; j++) buf[p++] = tmp[j];
        if (i < 3) buf[p++] = '.';
    } buf[p] = 0;
    w_label(x, y, "IP:"); w_label_dim(x + 40, y, buf);
}
static void render_security(app_ctx_t* c) {
    int x = c->origin_x + 200, y = c->origin_y + 16;
    w_label_color(x, y, "Security", theme()->accent_hi); y += 28;
    w_label(x, y, "Password hashing: SHA-256 (FIPS)"); y += 20;
    w_label(x, y, "Ring separation: ring 0 + ring 3"); y += 20;
    w_label(x, y, "TSS active for syscalls");
}
static void render_storage(app_ctx_t* c) {
    int x = c->origin_x + 200, y = c->origin_y + 16;
    w_label_color(x, y, "Storage", theme()->accent_hi); y += 28;
    w_label(x, y, "Internal disk: ATA primary slave"); y += 20;
    w_label(x, y, "Sector size: 512 bytes"); y += 20;
    w_label(x, y, "Filesystem: VyFS (in-memory)");
}
static void render_about(app_ctx_t* c) {
    int x = c->origin_x + 200, y = c->origin_y + 16;
    const theme_t* t = theme();
    w_label_color(x, y, "About Vyro OS", t->accent_hi); y += 32;

    struct { const char* k; const char* v; } rows[] = {
        { "Version",    "2.0.0 (v8)"          },
        { "Architecture","x86_64 (64-bit)"    },
        { "Kernel",     "from-scratch custom"  },
        { "Boot",       "BIOS + VBE (GRUB-less)"},
        { "GUI",        "double-buffered compositor"},
        { "Network",    "TCP/IP + TLS 1.3"    },
        { "Crypto",     "9 primitives (RFC verified)"},
        { "License",    "MIT ($0 budget)"      },
    };
    for (int i = 0; i < 8; i++) {
        w_label(x, y, rows[i].k);
        w_label_color(x + 180, y, rows[i].v, t->accent_hi);
        y += 26;
    }
    y += 8; w_separator(x, y, 360); y += 16;
    char b[12]; int cn = cpu_count(); b[0]='0'+(char)cn; b[1]=' '; b[2]='C'; b[3]='P'; b[4]='U'; b[5]='s'; b[6]=' '; b[7]='d'; b[8]='e'; b[9]='t'; b[10]='e'; b[11]=0;
    w_label_dim(x, y, b); y += 20;
    w_label_dim(x, y, "155+ git tags  |  ~53,000 lines of code");
}

static void render_settings(app_ctx_t* c) {
    const theme_t* t = theme();

    comp_rect(c->origin_x, c->origin_y, 180, c->height, t->dock_bg);
    for (int i = 0; i < N_SECTIONS; i++) {
        int sx = c->origin_x + 8, sy = c->origin_y + 8 + i * 32;
        int sw = 164, sh = 28;
        if (w_list_item(sx, sy, sw, sh, sections[i], i == sel_section,
                         c->mx + c->origin_x, c->my + c->origin_y, c->clicked))
            sel_section = i;
    }

    comp_rect(c->origin_x + 180, c->origin_y, c->width - 180, c->height, t->win_body);
    switch (sel_section) {
        case 0: render_general(c); break;
        case 1: render_display(c); break;
        case 2: render_personal(c); break;
        case 3: render_accounts(c); break;
        case 4: render_network(c); break;
        case 5: render_security(c); break;
        case 6: render_storage(c); break;
        case 7: render_about(c); break;
    }
}

const app_def_t APP_SETTINGS = { "Settings", 'S', 0x9696A0, render_settings, 640, 480 };
