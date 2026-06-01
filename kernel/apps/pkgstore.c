#include "apps.h"
#include "../widgets.h"
#include "../theme.h"
#include "../compositor.h"
#include "../pkg.h"
#include "../notify.h"

static void render_pkgstore(app_ctx_t* c) {
    const theme_t* t = theme();
    comp_rect(c->origin_x, c->origin_y, c->width, c->height, t->win_body);
    w_label_color(c->origin_x + 16, c->origin_y + 12, "Vyro App Store", t->accent_hi);
    w_separator(c->origin_x + 16, c->origin_y + 36, c->width - 32);

    int abs_mx = c->mx + c->origin_x, abs_my = c->my + c->origin_y;
    int n; package_t* r = pkg_repo(&n);
    int y = c->origin_y + 50;
    for (int i = 0; i < n && y < c->origin_y + c->height - 50; i++) {
        comp_rect(c->origin_x + 16, y, c->width - 32, 46, t->dock_bg);
        comp_border(c->origin_x + 16, y, c->width - 32, 46, t->win_border);
        w_label(c->origin_x + 28, y + 6, r[i].name);
        char ver[16] = "v";
        int vp = 1; for (int j = 0; r[i].version[j] && vp < 14; j++) ver[vp++] = r[i].version[j];
        ver[vp] = 0;
        w_label_dim(c->origin_x + 28 + 90, y + 6, ver);
        w_label_dim(c->origin_x + 28, y + 24, r[i].desc);

        int bx = c->origin_x + c->width - 100, by = y + 10;
        if (r[i].installed) {
            comp_rect(bx, by, 80, 28, t->success);
            comp_text(bx + 8, by + 6, "Installed", 0xFFFFFF, t->success);
        } else {
            if (w_button(bx, by, 80, 28, "Install",
                         abs_mx, abs_my, c->clicked)) {
                int k = pkg_install(r[i].name);
                if (k > 0) {
                    char msg[40] = "installed "; int p = 10;
                    for (int j = 0; r[i].name[j] && p < 38; j++) msg[p++] = r[i].name[j];
                    msg[p] = 0;
                    notify_post("Package", msg);
                }
            }
        }
        y += 50;
    }
}

const app_def_t APP_PKGSTORE = { "App Store", '*', 0x80FF80, render_pkgstore, 560, 480 };
