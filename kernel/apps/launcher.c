#include "apps.h"
#include "../widgets.h"
#include "../theme.h"
#include "../compositor.h"

static void (*launch_cb)(const char*) = 0;
void launcher_set_callback(void (*cb)(const char*)) { launch_cb = cb; }

static void render_launcher(app_ctx_t* c) {
    const theme_t* t = theme();
    comp_rect(c->origin_x, c->origin_y, c->width, c->height, t->win_body);
    w_label_color(c->origin_x + 16, c->origin_y + 12, "Applications", t->accent_hi);
    w_separator(c->origin_x + 16, c->origin_y + 36, c->width - 32);

    int abs_mx = c->mx + c->origin_x, abs_my = c->my + c->origin_y;
    int cols = 4;
    int tile = 80;
    int gx0 = c->origin_x + 30;
    int gy0 = c->origin_y + 60;

    int n = app_count();
    for (int i = 0; i < n; i++) {
        const app_def_t* a = app_get(i);
        int row = i / cols, col = i % cols;
        int x = gx0 + col * (tile + 20);
        int y = gy0 + row * (tile + 30);
        if (w_icon_tile(x, y, tile, a->name, a->icon_glyph, a->icon_color,
                        abs_mx, abs_my, c->clicked)) {
            if (launch_cb) launch_cb(a->name);
        }
    }
}

const app_def_t APP_LAUNCHER = { "Launchpad", 'A', 0x60C880, render_launcher, 480, 400 };
