#include "widgets.h"
#include "compositor.h"
#include "theme.h"

static int hit(int px, int py, int x, int y, int w, int h) {
    return px >= x && px < x + w && py >= y && py < y + h;
}
static int slen(const char* s) { int i=0; while(s[i]) i++; return i; }

int w_button(int x, int y, int w, int h, const char* label,
             int mx, int my, int clicked) {
    const theme_t* t = theme();
    int hover = hit(mx, my, x, y, w, h);
    uint32_t bg = hover ? t->accent_hi : t->accent;
    comp_rect(x, y, w, h, bg);
    comp_border(x, y, w, h, t->win_border);
    int len = slen(label);
    int tx = x + (w - len * 8) / 2;
    int ty = y + (h - 16) / 2;
    comp_text(tx, ty, label, 0xFFFFFF, bg);
    return hover && clicked;
}

void w_label(int x, int y, const char* text) {
    const theme_t* t = theme();
    comp_text_bg_alpha(x, y, text, t->text);
}
void w_label_dim(int x, int y, const char* text) {
    const theme_t* t = theme();
    comp_text_bg_alpha(x, y, text, t->text_dim);
}
void w_label_color(int x, int y, const char* text, uint32_t color) {
    comp_text_bg_alpha(x, y, text, color);
}

void w_panel(int x, int y, int w, int h) {
    const theme_t* t = theme();
    comp_rect(x, y, w, h, t->win_body);
    comp_border(x, y, w, h, t->win_border);
}

void w_panel_titled(int x, int y, int w, int h, const char* title) {
    const theme_t* t = theme();
    comp_rect(x, y, w, h, t->win_body);
    comp_rect(x, y, w, 24, t->win_title);
    comp_border(x, y, w, h, t->win_border);
    comp_text(x + 8, y + 4, title, t->text, t->win_title);
}

void w_separator(int x, int y, int w) {
    const theme_t* t = theme();
    for (int i = 0; i < w; i++) comp_pixel(x + i, y, t->win_border);
}

void w_progress(int x, int y, int w, int h, int pct) {
    const theme_t* t = theme();
    if (pct < 0) pct = 0; if (pct > 100) pct = 100;
    comp_rect(x, y, w, h, t->dock_bg);
    comp_rect(x, y, w * pct / 100, h, t->accent);
    comp_border(x, y, w, h, t->win_border);
}

int w_toggle(int x, int y, int state, int mx, int my, int clicked) {
    const theme_t* t = theme();
    int w = 40, h = 20;
    int hover = hit(mx, my, x, y, w, h);
    if (hover && clicked) state = !state;
    comp_rect(x, y, w, h, state ? t->accent : t->dock_bg);
    comp_border(x, y, w, h, t->win_border);
    int knob_x = state ? x + w - h + 2 : x + 2;
    comp_rect(knob_x, y + 2, h - 4, h - 4, 0xFFFFFF);
    return state;
}

void w_input(int x, int y, int w, const char* text, int focused) {
    const theme_t* t = theme();
    comp_rect(x, y, w, 24, t->win_body);
    comp_border(x, y, w, 24, focused ? t->accent : t->win_border);
    comp_text(x + 6, y + 4, text, t->text, t->win_body);
    if (focused) {
        int cx = x + 6 + slen(text) * 8;
        comp_rect(cx, y + 4, 1, 16, t->text);
    }
}

int w_list_item(int x, int y, int w, int h, const char* text,
                int selected, int mx, int my, int clicked) {
    const theme_t* t = theme();
    int hover = hit(mx, my, x, y, w, h);
    uint32_t bg = selected ? t->accent : (hover ? t->win_title : t->win_body);
    comp_rect(x, y, w, h, bg);
    comp_text(x + 8, y + (h - 16) / 2, text,
              selected ? 0xFFFFFF : t->text, bg);
    return hover && clicked;
}

int w_icon_tile(int x, int y, int size, const char* label,
                char icon_glyph, uint32_t icon_color,
                int mx, int my, int clicked) {
    const theme_t* t = theme();
    int hover = hit(mx, my, x, y, size, size + 18);
    uint32_t bg = hover ? t->accent : t->win_body;
    comp_rect(x, y, size, size, bg);
    comp_border(x, y, size, size, t->win_border);

    comp_glyph(x + size/2 - 4, y + size/2 - 8,
               icon_glyph, hover ? 0xFFFFFF : icon_color, bg);
    int len = slen(label);
    int tx = x + (size - len * 8) / 2;
    w_label(tx, y + size + 4, label);
    return hover && clicked;
}
