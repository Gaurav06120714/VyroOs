#include "apps.h"
#include "../widgets.h"
#include "../theme.h"
#include "../compositor.h"

#define TE_MAX 1024
static char  text[TE_MAX] = "Welcome to VyroEdit.\n\nA simple text editor running on the\nVyro OS desktop. Click here, then type!";
static int   text_len = 0;
static int   initted = 0;

static void render_textedit(app_ctx_t* c) {
    if (!initted) { text_len = 0; while (text[text_len]) text_len++; initted = 1; }
    const theme_t* t = theme();
    // Toolbar
    comp_rect(c->origin_x, c->origin_y, c->width, 32, t->win_title);
    int abs_mx = c->mx + c->origin_x, abs_my = c->my + c->origin_y;
    if (w_button(c->origin_x + 6, c->origin_y + 4, 60, 24, "New", abs_mx, abs_my, c->clicked)) {
        text[0] = 0; text_len = 0;
    }
    if (w_button(c->origin_x + 72, c->origin_y + 4, 60, 24, "Clear", abs_mx, abs_my, c->clicked)) {
        text[0] = 0; text_len = 0;
    }
    char info[24] = "chars: ";
    int v = text_len, p = 7;
    if (v == 0) info[p++] = '0';
    else { char r[6]; int n = 0; while (v) { r[n++] = '0' + v%10; v/=10; }
           while (n) info[p++] = r[--n]; }
    info[p] = 0;
    w_label_dim(c->origin_x + c->width - 90, c->origin_y + 8, info);

    // Editor area
    comp_rect(c->origin_x, c->origin_y + 32, c->width, c->height - 32, 0xF8FAF8);
    int x = c->origin_x + 10;
    int y = c->origin_y + 40;
    for (int i = 0; i < text_len; i++) {
        char ch = text[i];
        if (ch == '\n') { x = c->origin_x + 10; y += 18; continue; }
        if (x > c->origin_x + c->width - 12) { x = c->origin_x + 10; y += 18; }
        comp_glyph(x, y, ch, 0x202028, 0xF8FAF8);
        x += 8;
    }
    // Caret at end
    if (y < c->origin_y + c->height - 20) comp_rect(x, y, 2, 16, 0x202028);

    // Input handling
    if (c->key) {
        if (c->key == '\b' && text_len > 0) text[--text_len] = 0;
        else if ((c->key >= 32 && c->key < 127) || c->key == '\n') {
            if (text_len < TE_MAX - 1) { text[text_len++] = (char)c->key; text[text_len] = 0; }
        }
    }
    (void)t;
}

const app_def_t APP_TEXTEDIT = { "TextEdit", 'E', 0xE0C040, render_textedit, 560, 400 };
