#include "apps.h"
#include "../widgets.h"
#include "../theme.h"
#include "../compositor.h"
#include "../../drivers/rtc.h"
#include "../../drivers/timer.h"

static void d2(char* b, int n) { b[0] = '0' + n/10; b[1] = '0' + n%10; }

static const char* months[] = {"","Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};

static void render_clock(app_ctx_t* c) {
    const theme_t* t = theme();
    comp_rect(c->origin_x, c->origin_y, c->width, c->height, t->win_body);
    rtc_time_t rt; rtc_read(&rt);
    char clk[12] = "00:00:00";
    d2(clk, rt.hour); d2(clk+3, rt.minute); d2(clk+6, rt.second);

    // Big clock — draw each char 3x size
    int cx = c->origin_x + 40, cy = c->origin_y + 60;
    for (int i = 0; i < 8; i++) {
        for (int dy = 0; dy < 3; dy++)
            for (int dx = 0; dx < 3; dx++)
                comp_glyph(cx + i * 28 + dx, cy + dy, clk[i], t->accent_hi, t->win_body);
    }
    // Date below
    char dt[32] = "Mon Jan 01 2026";
    const char* mo = (rt.month >= 1 && rt.month <= 12) ? months[rt.month] : "???";
    int p = 0;
    dt[p++] = mo[0]; dt[p++] = mo[1]; dt[p++] = mo[2]; dt[p++] = ' ';
    d2(dt+p, rt.day); p += 2; dt[p++] = ',';  dt[p++] = ' ';
    int y = rt.year;
    dt[p++] = '0' + (y/1000)%10;
    dt[p++] = '0' + (y/100)%10;
    dt[p++] = '0' + (y/10)%10;
    dt[p++] = '0' + y%10;
    dt[p] = 0;
    w_label_dim(c->origin_x + 100, cy + 80, dt);

    char up[40] = "Uptime: ";
    long s = (long)(timer_uptime_seconds());
    int up_p = 8;
    char rev[12]; int rn = 0; long v = s;
    if (v == 0) rev[rn++] = '0';
    else while (v) { rev[rn++] = '0' + v%10; v /= 10; }
    while (rn) up[up_p++] = rev[--rn];
    up[up_p++] = ' '; up[up_p++] = 's'; up[up_p] = 0;
    w_label_dim(c->origin_x + 100, cy + 100, up);
}

const app_def_t APP_CLOCK = { "Clock", 'O', 0x60D0FF, render_clock, 400, 220 };
