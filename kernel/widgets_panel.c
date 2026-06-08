#include "widgets_panel.h"
#include "compositor.h"
#include "theme.h"
#include "../drivers/rtc.h"
#include "../drivers/timer.h"

static void glass_panel(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    const theme_t* t = theme();

    comp_shadow(x, y, w, h, t->win_shadow);

    uint32_t body = t->is_dark ? 0x202838 : 0xF0F2F8;
    comp_rect(x, y, w, h, body);

    uint32_t highlight = t->is_dark ? 0x40506A : 0xFFFFFF;
    for (uint32_t i = 0; i < w; i++) comp_pixel(x + i, y, highlight);

    comp_border(x, y, w, h, t->is_dark ? 0x3A4258 : 0xC8D0DC);
}

static void draw_ring(uint32_t cx, uint32_t cy, uint32_t r, uint32_t thickness,
                     int pct, uint32_t color, uint32_t bg) {




    int filled_arc_deg = pct * 360 / 100;
    for (int dy = -(int)r; dy <= (int)r; dy++) {
        for (int dx = -(int)r; dx <= (int)r; dx++) {
            int d2 = dx*dx + dy*dy;
            int r2 = (int)r * (int)r;
            int rin = (int)(r - thickness);
            int rin2 = rin * rin;
            if (d2 <= r2 && d2 >= rin2) {



                int color_pixel = 0;

                int deg;
                if (dx == 0 && dy < 0) deg = 0;
                else if (dx > 0 && dy < 0) deg = 45;
                else if (dx > 0 && dy == 0) deg = 90;
                else if (dx > 0 && dy > 0) deg = 135;
                else if (dx == 0 && dy > 0) deg = 180;
                else if (dx < 0 && dy > 0) deg = 225;
                else if (dx < 0 && dy == 0) deg = 270;
                else deg = 315;
                if (deg < filled_arc_deg) color_pixel = 1;
                comp_pixel(cx + dx, cy + dy, color_pixel ? color : bg);
            }
        }
    }
}

static void big_glyph(uint32_t x, uint32_t y, char c, uint32_t fg, uint32_t bg) {
    uint8_t* font = (uint8_t*)0x80000;
    uint8_t gi = (uint8_t)c;
    for (int gy = 0; gy < 16; gy++) {
        uint8_t row = font[gi * 16 + gy];
        for (int gx = 0; gx < 8; gx++) {
            uint32_t col = (row & (0x80 >> gx)) ? fg : bg;
            comp_pixel(x + gx*2,     y + gy*2,     col);
            comp_pixel(x + gx*2 + 1, y + gy*2,     col);
            comp_pixel(x + gx*2,     y + gy*2 + 1, col);
            comp_pixel(x + gx*2 + 1, y + gy*2 + 1, col);
        }
    }
}
static void big_text(uint32_t x, uint32_t y, const char* s, uint32_t color, uint32_t bg) {
    for (int i = 0; s[i]; i++) big_glyph(x + i * 18, y, s[i], color, bg);
}

static void huge_glyph(uint32_t x, uint32_t y, char c, uint32_t fg, uint32_t bg) {
    uint8_t* font = (uint8_t*)0x80000;
    uint8_t gi = (uint8_t)c;
    for (int gy = 0; gy < 16; gy++) {
        uint8_t row = font[gi * 16 + gy];
        for (int gx = 0; gx < 8; gx++) {
            uint32_t col = (row & (0x80 >> gx)) ? fg : bg;
            for (int dy = 0; dy < 3; dy++)
                for (int dx = 0; dx < 3; dx++)
                    comp_pixel(x + gx*3 + dx, y + gy*3 + dy, col);
        }
    }
}
static void huge_text(uint32_t x, uint32_t y, const char* s, uint32_t color, uint32_t bg) {
    for (int i = 0; s[i]; i++) huge_glyph(x + i * 26, y, s[i], color, bg);
}

static void d2(char* b, int n) { b[0] = '0' + n/10; b[1] = '0' + n%10; }
static int  int_to_str(uint64_t n, char* buf) {
    if (n == 0) { buf[0] = '0'; buf[1] = 0; return 1; }
    char rev[24]; int r = 0;
    while (n) { rev[r++] = '0' + n%10; n /= 10; }
    for (int i = 0; i < r; i++) buf[i] = rev[r-1-i];
    buf[r] = 0; return r;
}

static const char* months[] = {"","Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
static const char* dow[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};

static int day_of_week(int y, int m, int d) {
    if (m < 3) { m += 12; y -= 1; }
    int K = y % 100, J = y / 100;
    int h = (d + (13*(m+1))/5 + K + K/4 + J/4 + 5*J) % 7;

    return (h + 6) % 7;
}

static void widget_battery(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    glass_panel(x, y, w, h);
    const theme_t* t = theme();
    uint32_t bgc = t->is_dark ? 0x202838 : 0xF0F2F8;
    uint32_t green = 0x60D070;
    uint32_t bg = t->is_dark ? 0x303848 : 0xE0E5EC;

    uint32_t cx = x + 46, cy = y + h/2;
    draw_ring(cx, cy, 28, 6, 100, green, bg);

    big_text(cx - 20, cy - 8, "100", t->text, bgc);

    comp_text_bg_alpha(x + 100, y + 18, "Battery", t->text);
    big_text(x + 100, y + 36, "100%", green, bgc);
}

static void widget_clock(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    glass_panel(x, y, w, h);
    const theme_t* t = theme();
    uint32_t bgc = t->is_dark ? 0x202838 : 0xF0F2F8;
    rtc_time_t rt; rtc_read(&rt);
    char clk[6]; d2(clk, rt.hour); clk[2] = ':'; d2(clk+3, rt.minute); clk[5] = 0;
    int sx = x + w/2 - 65;
    huge_text(sx, y + 14, clk, t->text, bgc);
    comp_text_bg_alpha(x + 14, y + h - 22, "Clock", t->text_dim);
}

static void widget_calendar(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    glass_panel(x, y, w, h);
    const theme_t* t = theme();
    uint32_t bgc = t->is_dark ? 0x202838 : 0xF0F2F8;
    rtc_time_t rt; rtc_read(&rt);


    int dw = day_of_week(rt.year, rt.month, rt.day);
    comp_text_bg_alpha(x + 14, y + 14, dow[dw], 0xFF6060);

    const char* mo = (rt.month >= 1 && rt.month <= 12) ? months[rt.month] : "???";
    comp_text_bg_alpha(x + 14, y + 34, mo, t->text_dim);


    char buf[3];
    if (rt.day >= 10) { d2(buf, rt.day); buf[2] = 0; }
    else { buf[0] = '0' + rt.day; buf[1] = 0; }
    huge_text(x + 14, y + 52, buf, t->text, bgc);
}

static void widget_weather(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    glass_panel(x, y, w, h);
    const theme_t* t = theme();
    uint32_t bgc = t->is_dark ? 0x202838 : 0xF0F2F8;
    comp_text_bg_alpha(x + 14, y + 14, "Bhongir", t->text);
    comp_text_bg_alpha(x + 14, y + 34, "Mostly Sunny", t->text_dim);
    huge_text(x + 14, y + 52, "28", t->text, bgc);
    comp_text_bg_alpha(x + 62, y + 56, "C", 0xFFC040);
    comp_text_bg_alpha(x + 14, y + h - 22, "H:36 L:27", t->text_dim);
}

static void widget_activity(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    glass_panel(x, y, w, h);
    const theme_t* t = theme();
    uint32_t bgc = t->is_dark ? 0x202838 : 0xF0F2F8;
    comp_text_bg_alpha(x + 14, y + 14, "Activity", t->text);
    uint64_t up_min = timer_uptime_seconds() / 60;
    if (up_min == 0) up_min = 1;
    char buf[16];
    int digits = int_to_str(up_min, buf);
    huge_text(x + 14, y + 34, buf, 0xFFC040, bgc);
    comp_text_bg_alpha(x + 14 + 26 * digits + 6, y + 60, "min", t->text_dim);

    int gx = x + 14, gy = y + h - 32;
    for (int b = 0; b < 18; b++) {
        int bh = 6 + ((b * 13 + 7) % 22);
        comp_rect(gx + b * 11, gy + (22 - bh), 8, bh, 0xFFC040);
    }
}

static void widget_map(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    glass_panel(x, y, w, h);
    const theme_t* t = theme();

    uint32_t road = t->is_dark ? 0x3A4258 : 0xD0D8E4;
    for (int i = 8; i < (int)w - 8; i += 18)
        comp_rect(x + i, y + 12, 2, h - 36, road);
    for (int j = 12; j < (int)h - 24; j += 24)
        comp_rect(x + 8, y + j, w - 16, 2, road);

    int dx_ = x + w/2, dy_ = y + h/2 - 6;
    comp_rect(dx_ - 5, dy_ - 5, 10, 10, t->accent);

    comp_border(dx_ - 6, dy_ - 6, 12, 12, 0xFFFFFF);
    comp_text_bg_alpha(x + 12, y + h - 22, "Location", t->text);
}

void widgets_panel_draw(uint32_t px, uint32_t py, uint32_t pw) {
    uint32_t card_w = pw - 16;
    uint32_t y = py + 10;


    widget_battery (px + 8, y, card_w, 80);  y += 88;
    widget_activity(px + 8, y, card_w, 100); y += 108;

    widget_map     (px + 8, y, card_w, 110); y += 118;

    widget_clock   (px + 8, y, card_w, 80);  y += 88;

    widget_calendar(px + 8, y, card_w / 2 - 4, 110);
    widget_weather (px + 8 + card_w/2 + 4, y, card_w / 2 - 4, 110);
    (void)big_text;
}
