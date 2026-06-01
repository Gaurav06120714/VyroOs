#include "apps.h"
#include "../widgets.h"
#include "../theme.h"
#include "../compositor.h"
#include "../notify.h"

static void render_notes(app_ctx_t* c) {
    const theme_t* t = theme();
    comp_rect(c->origin_x, c->origin_y, c->width, c->height, t->win_body);
    w_label_color(c->origin_x + 16, c->origin_y + 12, "Notification Center", t->accent_hi);
    w_separator(c->origin_x + 16, c->origin_y + 36, c->width - 32);

    notification_t* hist; int n;
    notify_history(&hist, &n);
    int y = c->origin_y + 50;
    int shown = 0;
    for (int i = n - 1; i >= 0 && shown < 8; i--) {
        comp_rect(c->origin_x + 16, y, c->width - 32, 44, t->dock_bg);
        comp_rect(c->origin_x + 16, y, 4, 44, t->accent);
        comp_border(c->origin_x + 16, y, c->width - 32, 44, t->win_border);
        w_label(c->origin_x + 28, y + 6, hist[i].title);
        w_label_dim(c->origin_x + 28, y + 24, hist[i].body);
        y += 50; shown++;
    }
    if (shown == 0) w_label_dim(c->origin_x + 16, y, "No notifications.");
}

const app_def_t APP_NOTECENTER = { "Notifications", 'N', 0xFFC040, render_notes, 480, 480 };
