#include "apps.h"
#include "../widgets.h"
#include "../theme.h"
#include "../compositor.h"
#include "../notify.h"

static int wifi_on = 1, bt_on = 0, dnd_on = 0, airdrop_on = 1;
static int brightness = 80, volume = 60;

static void tile(int x, int y, const char* label, int* state,
                 int abs_mx, int abs_my, int clicked) {
    const theme_t* t = theme();
    int w = 100, h = 70;
    int hit = (abs_mx >= x && abs_mx < x+w && abs_my >= y && abs_my < y+h);
    if (hit && clicked) { *state = !*state; notify_post(label, *state ? "On" : "Off"); }
    uint32_t bg = *state ? t->accent : t->dock_bg;
    comp_rect(x, y, w, h, bg);
    comp_border(x, y, w, h, t->win_border);
    w_label_color(x + 10, y + 12, label, *state ? 0xFFFFFF : t->text);
    w_label_color(x + 10, y + 44, *state ? "On" : "Off",
                  *state ? 0xFFFFFF : t->text_dim);
}

static void render_control(app_ctx_t* c) {
    const theme_t* t = theme();
    comp_rect(c->origin_x, c->origin_y, c->width, c->height, t->win_body);
    w_label_color(c->origin_x + 16, c->origin_y + 12, "Control Center", t->accent_hi);
    w_separator(c->origin_x + 16, c->origin_y + 36, c->width - 32);

    int abs_mx = c->mx + c->origin_x, abs_my = c->my + c->origin_y;
    int x = c->origin_x + 16, y = c->origin_y + 50;
    tile(x, y, "Wi-Fi", &wifi_on, abs_mx, abs_my, c->clicked);
    tile(x + 116, y, "Bluetooth", &bt_on, abs_mx, abs_my, c->clicked);
    tile(x + 232, y, "Do Not Disturb", &dnd_on, abs_mx, abs_my, c->clicked);
    tile(x + 348, y, "AirDrop", &airdrop_on, abs_mx, abs_my, c->clicked);

    y += 90;
    w_label(x, y, "Brightness"); w_progress(x, y + 18, c->width - 32, 14, brightness);
    y += 50;
    w_label(x, y, "Volume");     w_progress(x, y + 18, c->width - 32, 14, volume);
}

const app_def_t APP_CONTROL = { "Control Center", 'C', 0x80E0FF, render_control, 500, 280 };
