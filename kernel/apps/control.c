#include "apps.h"
#include "../widgets.h"
#include "../theme.h"
#include "../compositor.h"
#include "../notify.h"

static int wifi_on    = 1;
static int bt_on      = 0;
static int dnd_on     = 0;
static int airdrop_on = 1;
static int dark_on    = 1;
static int anim_on    = 1;
static int brightness = 80;
static int volume     = 60;

static void draw_tile(int x, int y, int w, int h,
                      const char* label, const char* sub, int* state,
                      uint32_t icon_col,
                      int abs_mx, int abs_my, int clicked) {
    const theme_t* t = theme();
    int hit = (abs_mx >= x && abs_mx < x+w && abs_my >= y && abs_my < y+h);
    if (hit && clicked) {
        *state = !*state;
        notify_post(label, *state ? "Enabled" : "Disabled");
    }
    int hov = hit && !clicked;
    uint32_t bg = *state ? icon_col : (hov ? (t->is_dark ? 0x252D3A : 0xDDE3F0) : t->dock_bg);
    comp_rect(x, y, w, h, bg);
    comp_border(x, y, w, h, *state ? icon_col : t->win_border);

    /* icon circle */
    uint32_t ic = *state ? 0xFFFFFF : icon_col;
    for (int dy=-10;dy<=10;dy++) for(int dx=-10;dx<=10;dx++)
        if (dx*dx+dy*dy<=100) comp_pixel(x+w/2+dx, y+18+dy, ic);

    comp_text(x + w/2 - 4, y + 12, *state ? "O" : "O",
              *state ? 0xFFFFFF : icon_col, bg);

    /* label */
    int ll = 0; while(label[ll]) ll++;
    comp_text(x + w/2 - ll*4, y + 36, label,
              *state ? 0xFFFFFF : t->text, bg);
    int sl = 0; while(sub[sl]) sl++;
    comp_text(x + w/2 - sl*4, y + 52, sub,
              *state ? 0xDDEEFF : t->text_dim, bg);
}

static void draw_slider_row(app_ctx_t* c, int y, const char* label,
                             int* value, int abs_mx, int abs_my, int clicked) {
    const theme_t* t = theme();
    int x = c->origin_x + 16;
    int sw = c->width - 32;

    w_label(x, y, label);
    char vb[8]; int vv = *value, vp = 0;
    if (!vv) { vb[0]='0'; vb[1]='%'; vb[2]=0; }
    else {
        char r[4]; int n=0; while(vv){r[n++]='0'+vv%10;vv/=10;}
        while(n) vb[vp++]=r[--n]; vb[vp++]='%'; vb[vp]=0;
    }
    w_label_color(x + sw - 32, y, vb, t->accent_hi);
    y += 20;

    int bw = 28;
    w_progress(x, y + 3, sw - 70, 14, *value);
    if (w_button(x + sw - 64, y, bw, 20, "-", abs_mx, abs_my, clicked) && *value > 0)   *value -= 5;
    if (w_button(x + sw - 32, y, bw, 20, "+", abs_mx, abs_my, clicked) && *value < 100) *value += 5;
}

static void render_control(app_ctx_t* c) {
    const theme_t* t = theme();
    int abs_mx = c->mx + c->origin_x, abs_my = c->my + c->origin_y;

    uint32_t bg = t->is_dark ? 0x0D1117 : 0xF4F6FA;
    comp_rect(c->origin_x, c->origin_y, c->width, c->height, bg);

    /* header */
    uint32_t hbg = t->is_dark ? 0x1C2230 : 0xEEF1F8;
    comp_rect(c->origin_x, c->origin_y, c->width, 38, hbg);
    comp_rect(c->origin_x, c->origin_y + 37, c->width, 1, t->win_border);
    w_label_color(c->origin_x + 16, c->origin_y + 12, "Control Center", t->accent_hi);

    /* tiles row 1 */
    int tile_w = (c->width - 32 - 12) / 4;
    int tile_h = 76;
    int tx0 = c->origin_x + 16;
    int ty0 = c->origin_y + 50;

    struct { const char* l; const char* s; int* st; uint32_t col; } tiles[4];
    tiles[0].l="Wi-Fi";  tiles[0].s=wifi_on?"On":"Off";     tiles[0].st=&wifi_on;    tiles[0].col=0x4080FF;
    tiles[1].l="Bluetooth"; tiles[1].s=bt_on?"On":"Off";    tiles[1].st=&bt_on;      tiles[1].col=0x6040C0;
    tiles[2].l="DND";    tiles[2].s=dnd_on?"Active":"Off";  tiles[2].st=&dnd_on;     tiles[2].col=0xFF6040;
    tiles[3].l="AirDrop";tiles[3].s=airdrop_on?"On":"Off";  tiles[3].st=&airdrop_on; tiles[3].col=0x40C080;

    for (int i = 0; i < 4; i++) {
        int tx = tx0 + i * (tile_w + 4);
        draw_tile(tx, ty0, tile_w, tile_h,
                  tiles[i].l, tiles[i].s, tiles[i].st, tiles[i].col,
                  abs_mx, abs_my, c->clicked);
    }

    /* tiles row 2 */
    int ty1 = ty0 + tile_h + 8;
    struct { const char* l; const char* s; int* st; uint32_t col; } tiles2[2];
    tiles2[0].l="Dark Mode";   tiles2[0].s=dark_on?"On":"Off";  tiles2[0].st=&dark_on;  tiles2[0].col=0x3A4A6A;
    tiles2[1].l="Animations";  tiles2[1].s=anim_on?"On":"Off";  tiles2[1].st=&anim_on;  tiles2[1].col=0x20A060;

    int tw2 = (c->width - 32 - 4) / 2;
    for (int i = 0; i < 2; i++) {
        int tx = tx0 + i * (tw2 + 4);
        draw_tile(tx, ty1, tw2, tile_h,
                  tiles2[i].l, tiles2[i].s, tiles2[i].st, tiles2[i].col,
                  abs_mx, abs_my, c->clicked);
    }
    if (dark_on != t->is_dark) theme_set_dark(dark_on);

    /* sliders */
    int sy = ty1 + tile_h + 16;
    comp_rect(c->origin_x, sy - 6, c->width, 1, t->win_border);
    sy += 4;

    draw_slider_row(c, sy, "Brightness", &brightness, abs_mx, abs_my, c->clicked);
    sy += 50;
    draw_slider_row(c, sy, "Volume",     &volume,     abs_mx, abs_my, c->clicked);
    sy += 50;

    comp_rect(c->origin_x, sy - 4, c->width, 1, t->win_border);
    sy += 8;
    w_label_dim(c->origin_x + 16, sy, "Changes apply immediately. Open Settings for advanced options.");
}

const app_def_t APP_CONTROL = { "Control Center", 'C', 0x80E0FF, render_control, 520, 400 };
