#include "widgets_panel.h"
#include "compositor.h"
#include "theme.h"
#include "../drivers/rtc.h"
#include "../drivers/timer.h"

// Glass panel — semi-translucent dark card with white border at low alpha
static void glass_panel(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    const theme_t* t = theme();
    // Drop shadow
    comp_shadow(x, y, w, h, t->win_shadow);
    // Card body
    uint32_t body = t->is_dark ? 0x202838 : 0xF0F2F8;
    comp_rect(x, y, w, h, body);
    // Subtle top highlight (1px) to give "glass" feeling
    uint32_t highlight = t->is_dark ? 0x40506A : 0xFFFFFF;
    for (uint32_t i = 0; i < w; i++) comp_pixel(x + i, y, highlight);
    // Border
    comp_border(x, y, w, h, t->is_dark ? 0x3A4258 : 0xC8D0DC);
}

// Ring (for battery, screen time, etc.)
static void draw_ring(uint32_t cx, uint32_t cy, uint32_t r, uint32_t thickness,
                     int pct, uint32_t color, uint32_t bg) {
    // Simple ring via two filled circles minus angular sweep — we approximate
    // with a filled circle for the "bg" and overlay only pct sweep pixels.
    // Cheap method: draw all pixels of radius r..r-thickness in 'bg', then
    // overwrite the first (pct*360/100) degrees in 'color'.
    int filled_arc_deg = pct * 360 / 100;
    for (int dy = -(int)r; dy <= (int)r; dy++) {
        for (int dx = -(int)r; dx <= (int)r; dx++) {
            int d2 = dx*dx + dy*dy;
            int r2 = (int)r * (int)r;
            int rin = (int)(r - thickness);
            int rin2 = rin * rin;
            if (d2 <= r2 && d2 >= rin2) {
                // Determine angle (0° = top, clockwise)
                // angle = atan2(dx, -dy) — use lookup-free approximation
                // We just compare via cross-multiplied region tests for simple percentages.
                int color_pixel = 0;
                // Approximate by checking which octant the point is in
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

// Large 2x-scaled text
static void big_text(uint32_t x, uint32_t y, const char* s, uint32_t color) {
    for (int i = 0; s[i]; i++) {
        // Draw glyph at 2x via two stacked copies (limited but readable)
        for (int dy = 0; dy < 2; dy++)
            for (int dx = 0; dx < 2; dx++)
                comp_glyph(x + i * 16 + dx, y + dy, s[i], color, 0xFFFFFFFF == 0 ? color : color);
    }
}

static void d2(char* b, int n) { b[0] = '0' + n/10; b[1] = '0' + n%10; }

static const char* months[] = {"","Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
static const char* dow[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};

// Rough Zeller-style day-of-week (0=Sun)
static int day_of_week(int y, int m, int d) {
    if (m < 3) { m += 12; y -= 1; }
    int K = y % 100, J = y / 100;
    int h = (d + (13*(m+1))/5 + K + K/4 + J/4 + 5*J) % 7;
    // h: 0=Sat..6=Fri → convert to 0=Sun..6=Sat
    return (h + 6) % 7;
}

// ── Widget: Battery ring at 100% ──
static void widget_battery(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    glass_panel(x, y, w, h);
    const theme_t* t = theme();
    uint32_t green = 0x60D070;
    uint32_t bg = t->is_dark ? 0x303848 : 0xE0E5EC;

    uint32_t cx = x + 50, cy = y + h/2;
    draw_ring(cx, cy, 30, 6, 100, green, bg);
    // Center "100%" text
    big_text(cx - 22, cy - 6, "100", t->text);
    comp_text_bg_alpha(cx + 26, cy - 4, "%", t->text);

    // Label
    comp_text_bg_alpha(x + 110, y + 16, "Battery", t->text);
    comp_text_bg_alpha(x + 110, y + 38, "Full charge", t->text_dim);
}

// ── Widget: Clock ──
static void widget_clock(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    glass_panel(x, y, w, h);
    const theme_t* t = theme();
    rtc_time_t rt; rtc_read(&rt);
    char clk[6]; d2(clk, rt.hour); clk[2] = ':'; d2(clk+3, rt.minute); clk[5] = 0;
    // 3x size
    int sx = x + w/2 - 36, sy = y + 18;
    for (int i = 0; i < 5; i++) {
        for (int dy = 0; dy < 3; dy++)
            for (int dx = 0; dx < 3; dx++)
                comp_glyph(sx + i * 16 + dx, sy + dy, clk[i], t->text, t->is_dark ? 0x202838 : 0xF0F2F8);
    }
    comp_text_bg_alpha(x + 12, y + h - 22, "Clock", t->text_dim);
}

// ── Widget: Calendar ──
static void widget_calendar(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    glass_panel(x, y, w, h);
    const theme_t* t = theme();
    rtc_time_t rt; rtc_read(&rt);

    // Day-of-week header
    int dw = day_of_week(rt.year, rt.month, rt.day);
    comp_text_bg_alpha(x + 14, y + 14, dow[dw], 0xFF6060);
    // Month
    const char* mo = (rt.month >= 1 && rt.month <= 12) ? months[rt.month] : "???";
    comp_text_bg_alpha(x + 14, y + 32, mo, t->text_dim);

    // Day number — big
    char buf[3]; d2(buf, rt.day); buf[2] = 0;
    int sx = x + 14, sy = y + 52;
    for (int i = 0; i < 2; i++) {
        for (int dy = 0; dy < 4; dy++)
            for (int dx = 0; dx < 4; dx++)
                comp_glyph(sx + i * 22 + dx, sy + dy, buf[i], t->text, t->is_dark ? 0x202838 : 0xF0F2F8);
    }
}

// ── Widget: Weather ──
static void widget_weather(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    glass_panel(x, y, w, h);
    const theme_t* t = theme();
    comp_text_bg_alpha(x + 14, y + 12, "Bhongir", t->text);
    comp_text_bg_alpha(x + 14, y + 32, "Mostly Sunny", t->text_dim);
    // Big temp
    char temp[5] = "28";
    int sx = x + 14, sy = y + 54;
    for (int i = 0; i < 2; i++) {
        for (int dy = 0; dy < 3; dy++)
            for (int dx = 0; dx < 3; dx++)
                comp_glyph(sx + i * 22 + dx, sy + dy, temp[i], t->text, t->is_dark ? 0x202838 : 0xF0F2F8);
    }
    comp_text_bg_alpha(sx + 50, sy + 6, "C", 0xFFC040);
    // H/L
    comp_text_bg_alpha(x + 14, y + h - 22, "H:36 L:27", t->text_dim);
}

// ── Widget: Activity (uptime as activity proxy) ──
static void widget_activity(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    glass_panel(x, y, w, h);
    const theme_t* t = theme();
    comp_text_bg_alpha(x + 14, y + 12, "Activity", t->text);
    uint64_t up_min = timer_uptime_seconds() / 60;
    char buf[16]; int p = 0;
    if (up_min == 0) buf[p++] = '0';
    else { char rev[8]; int n = 0; uint64_t v = up_min;
           while (v) { rev[n++] = '0' + v%10; v /= 10; }
           while (n) buf[p++] = rev[--n]; }
    buf[p++] = 'm'; buf[p] = 0;
    // big
    int sx = x + 14, sy = y + 30;
    for (int i = 0; buf[i]; i++) {
        for (int dy = 0; dy < 3; dy++)
            for (int dx = 0; dx < 3; dx++)
                comp_glyph(sx + i * 16 + dx, sy + dy, buf[i], 0xFFC040, t->is_dark ? 0x202838 : 0xF0F2F8);
    }
    // Bars graph
    int gx = x + 14, gy = y + h - 36;
    for (int b = 0; b < 12; b++) {
        int bh = 8 + ((b * 17) % 24);
        comp_rect(gx + b * 12, gy + (24 - bh), 8, bh, 0xFFC040);
    }
}

// ── Widget: Map placeholder ──
static void widget_map(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    glass_panel(x, y, w, h);
    const theme_t* t = theme();
    // Fake roads
    uint32_t road = t->is_dark ? 0x3A4258 : 0xD0D8E4;
    for (int i = 8; i < (int)w - 8; i += 18)
        comp_rect(x + i, y + 12, 2, h - 36, road);
    for (int j = 12; j < (int)h - 24; j += 24)
        comp_rect(x + 8, y + j, w - 16, 2, road);
    // Location dot
    int dx_ = x + w/2, dy_ = y + h/2 - 6;
    comp_rect(dx_ - 5, dy_ - 5, 10, 10, t->accent);
    // Border around dot
    comp_border(dx_ - 6, dy_ - 6, 12, 12, 0xFFFFFF);
    comp_text_bg_alpha(x + 12, y + h - 22, "Location", t->text);
}

// ── Public entrypoint ──
void widgets_panel_draw(uint32_t px, uint32_t py, uint32_t pw) {
    uint32_t card_w = pw - 16;
    uint32_t y = py + 10;

    // Battery + Activity row (small cards)
    widget_battery (px + 8, y, card_w, 80);  y += 88;
    widget_activity(px + 8, y, card_w, 100); y += 108;
    // Map
    widget_map     (px + 8, y, card_w, 110); y += 118;
    // Clock
    widget_clock   (px + 8, y, card_w, 80);  y += 88;
    // Calendar + Weather
    widget_calendar(px + 8, y, card_w / 2 - 4, 110);
    widget_weather (px + 8 + card_w/2 + 4, y, card_w / 2 - 4, 110);
    (void)big_text;
}
