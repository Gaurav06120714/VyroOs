#include "apps.h"
#include "../widgets.h"
#include "../theme.h"
#include "../compositor.h"
#include "../../drivers/rtc.h"

static const char* months[] = {"","January","February","March","April","May","June",
                               "July","August","September","October","November","December"};
static const char* dow_short[] = {"S","M","T","W","T","F","S"};

static int days_in_month(int m, int y) {
    static const int d[] = {0,31,28,31,30,31,30,31,31,30,31,30,31};
    if (m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) return 29;
    return d[m];
}
static int dow(int y, int m, int d) {
    if (m < 3) { m += 12; y -= 1; }
    int K = y % 100, J = y / 100;
    int h = (d + (13*(m+1))/5 + K + K/4 + J/4 + 5*J) % 7;
    return (h + 6) % 7;
}

static void render_calendar(app_ctx_t* c) {
    const theme_t* t = theme();
    comp_rect(c->origin_x, c->origin_y, c->width, c->height, t->win_body);

    rtc_time_t rt; rtc_read(&rt);


    comp_text(c->origin_x + 20, c->origin_y + 16,
              months[rt.month >= 1 && rt.month <= 12 ? rt.month : 1],
              t->accent_hi, t->win_body);
    char ybuf[8]; ybuf[0] = '0' + (rt.year/1000)%10;
    ybuf[1] = '0' + (rt.year/100)%10;
    ybuf[2] = '0' + (rt.year/10)%10;
    ybuf[3] = '0' + rt.year%10;
    ybuf[4] = 0;
    comp_text(c->origin_x + 120, c->origin_y + 16, ybuf, t->accent_hi, t->win_body);


    int cell_w = (c->width - 40) / 7;
    for (int i = 0; i < 7; i++) {
        int x = c->origin_x + 20 + i * cell_w + cell_w/2 - 4;
        comp_text(x, c->origin_y + 50, dow_short[i], t->text_dim, t->win_body);
    }


    int first_dow = dow(rt.year, rt.month, 1);
    int dim = days_in_month(rt.month, rt.year);
    int row = 0;
    for (int day = 1; day <= dim; day++) {
        int idx = first_dow + day - 1;
        int col = idx % 7;
        row = idx / 7;
        int cx = c->origin_x + 20 + col * cell_w;
        int cy = c->origin_y + 80 + row * 40;
        if (day == rt.day) {
            comp_rect(cx, cy, cell_w - 4, 36, t->accent);
        }
        char buf[3];
        if (day >= 10) { buf[0] = '0' + day/10; buf[1] = '0' + day%10; buf[2] = 0; }
        else { buf[0] = '0' + day; buf[1] = 0; }
        int tx = cx + cell_w/2 - (buf[1] ? 8 : 4);
        comp_text(tx, cy + 10, buf, day == rt.day ? 0xFFFFFF : t->text,
                  day == rt.day ? t->accent : t->win_body);
    }
}

const app_def_t APP_CALENDAR = { "Calendar", 'D', 0xFF6060, render_calendar, 480, 380 };
