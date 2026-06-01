#include "apps.h"
#include "../widgets.h"
#include "../theme.h"
#include "../compositor.h"
#include "../notify.h"
#include "../security.h"
#include "../net.h"
#include "../smp.h"

static int sel_section = 0;
static const char* sections[] = {
    "General", "Display", "Personalization", "Accounts",
    "Network", "Security", "Storage", "About"
};
#define N_SECTIONS 8

static void render_general(app_ctx_t* c) {
    int x = c->origin_x + 200;
    int y = c->origin_y + 16;
    w_label_color(x, y, "General", theme()->accent_hi); y += 28;
    w_label(x, y, "Theme:"); y += 20;
    int dark = theme()->is_dark;
    int new_dark = w_toggle(x + 80, y - 20, dark, c->mx + c->origin_x, c->my + c->origin_y, c->clicked);
    if (new_dark != dark) { theme_set_dark(new_dark); notify_post("Theme", new_dark?"Dark":"Light"); }
    w_label_dim(x, y, dark ? "Dark mode" : "Light mode"); y += 28;
    w_separator(x, y, 360); y += 16;
    w_label(x, y, "Sound volume:"); y += 20;
    w_progress(x, y, 360, 14, 80); y += 28;
    w_label_dim(x, y, "PC speaker output volume");
}
static void render_display(app_ctx_t* c) {
    int x = c->origin_x + 200, y = c->origin_y + 16;
    w_label_color(x, y, "Display", theme()->accent_hi); y += 28;
    w_label(x, y, "Resolution: 1024 x 768"); y += 20;
    w_label(x, y, "Color depth: 24 bpp BGR");  y += 20;
    w_label(x, y, "Framebuffer: VESA mode 0x118"); y += 20;
    w_label(x, y, "Refresh: ~60 Hz (compositor)");
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
    w_label_color(x, y, "Network", theme()->accent_hi); y += 28;
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
    w_label_color(x, y, "About Vyro OS", theme()->accent_hi); y += 28;
    w_label(x, y, "Version: 2.0.0"); y += 20;
    w_label(x, y, "Arch: x86_64 (64-bit)"); y += 20;
    w_label(x, y, "Kernel: monolithic, custom"); y += 20;
    char b[32]; int cn = cpu_count(); b[0] = '0' + cn; b[1] = ' '; b[2] = 'c'; b[3]='o'; b[4]='r'; b[5]='e'; b[6]='s'; b[7]=0;
    w_label(x, y, "CPUs:"); w_label_dim(x + 56, y, b); y += 20;
    w_label(x, y, "License: MIT"); y += 20;
    w_label_dim(x, y, "Built from scratch. $0 budget.");
}

static void render_settings(app_ctx_t* c) {
    const theme_t* t = theme();
    // Sidebar
    comp_rect(c->origin_x, c->origin_y, 180, c->height, t->dock_bg);
    for (int i = 0; i < N_SECTIONS; i++) {
        int sx = c->origin_x + 8, sy = c->origin_y + 8 + i * 32;
        int sw = 164, sh = 28;
        if (w_list_item(sx, sy, sw, sh, sections[i], i == sel_section,
                         c->mx + c->origin_x, c->my + c->origin_y, c->clicked))
            sel_section = i;
    }
    // Right pane
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
