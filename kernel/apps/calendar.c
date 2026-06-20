#include "apps.h"
#include "../widgets.h"
#include "../theme.h"
#include "../compositor.h"
#include "../../drivers/rtc.h"

static const char* months[] = {
    "","January","February","March","April","May","June",
    "July","August","September","October","November","December"
};
static const char* dow_names[] = { "Sun","Mon","Tue","Wed","Thu","Fri","Sat" };

static int view_month = 0;
static int view_year  = 0;
static int initted    = 0;
static int sel_day    = 0;

static int days_in_month(int m, int y) {
    static const int d[] = {0,31,28,31,30,31,30,31,31,30,31,30,31};
    if (m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) return 29;
    return d[m];
}
static int dow_of(int y, int m, int d) {
    if (m < 3) { m += 12; y -= 1; }
    int K = y % 100, J = y / 100;
    int h = (d + (13*(m+1))/5 + K + K/4 + J/4 + 5*J) % 7;
    return (h + 6) % 7;
}
static void num2(char* out, int v) {
    out[0] = '0' + v/10; out[1] = '0' + v%10; out[2] = 0;
}

static void render_calendar(app_ctx_t* c) {
    rtc_time_t rt; rtc_read(&rt);
    if (!initted) {
        view_month = rt.month; view_year = rt.year;
        sel_day    = rt.day;  initted = 1;
    }

    const theme_t* t = theme();
    int abs_mx = c->mx + c->origin_x, abs_my = c->my + c->origin_y;

    /* ---------- header bar ---------- */
    uint32_t hbg = t->is_dark ? 0x1C2230 : 0xEEF1F8;
    comp_rect(c->origin_x, c->origin_y, c->width, 40, hbg);
    comp_rect(c->origin_x, c->origin_y + 39, c->width, 1, t->win_border);

    /* prev / next month buttons */
    if (w_button(c->origin_x + 8, c->origin_y + 7, 28, 26, "<", abs_mx, abs_my, c->clicked)) {
        if (--view_month < 1) { view_month = 12; view_year--; }
        sel_day = 0;
    }
    if (w_button(c->origin_x + 42, c->origin_y + 7, 28, 26, ">", abs_mx, abs_my, c->clicked)) {
        if (++view_month > 12) { view_month = 1; view_year++; }
        sel_day = 0;
    }
    /* today button */
    if (w_button(c->origin_x + 76, c->origin_y + 7, 56, 26, "Today", abs_mx, abs_my, c->clicked)) {
        view_month = rt.month; view_year = rt.year; sel_day = rt.day;
    }
    /* month + year label */
    const char* mname = months[view_month >= 1 && view_month <= 12 ? view_month : 1];
    char ybuf[8]; ybuf[0]='0'+(view_year/1000)%10; ybuf[1]='0'+(view_year/100)%10;
    ybuf[2]='0'+(view_year/10)%10; ybuf[3]='0'+view_year%10; ybuf[4]=0;
    /* combine month year */
    char title[24]; int tp = 0;
    for (int i=0;mname[i];i++) title[tp++]=mname[i];
    title[tp++]=' ';
    for (int i=0;ybuf[i];i++) title[tp++]=ybuf[i];
    title[tp]=0;
    comp_text(c->origin_x + 148, c->origin_y + 12, title, t->accent_hi, hbg);

    /* ---------- day-of-week header ---------- */
    int cell_w = c->width / 7;
    int hdr_y  = c->origin_y + 42;
    uint32_t dhbg = t->is_dark ? 0x161B22 : 0xE4E8F0;
    comp_rect(c->origin_x, hdr_y, c->width, 24, dhbg);
    for (int i = 0; i < 7; i++) {
        int tx = c->origin_x + i * cell_w + cell_w/2 - 12;
        uint32_t dc = (i == 0 || i == 6) ? 0xFF7070 : t->text_dim;
        comp_text(tx, hdr_y + 5, dow_names[i], dc, dhbg);
    }

    /* ---------- grid ---------- */
    int grid_y = hdr_y + 26;
    int grid_h = c->height - grid_y - 60;
    int cell_h = grid_h / 6;
    uint32_t gbg = t->win_body;
    comp_rect(c->origin_x, grid_y, c->width, grid_h, gbg);

    int first = dow_of(view_year, view_month, 1);
    int dim   = days_in_month(view_month, view_year);
    int today_match = (view_month == rt.month && view_year == rt.year);

    for (int day = 1; day <= dim; day++) {
        int idx  = first + day - 1;
        int col  = idx % 7;
        int row  = idx / 7;
        int cx   = c->origin_x + col * cell_w;
        int cy   = grid_y + row * cell_h;
        int is_today = today_match && day == rt.day;
        int is_sel   = day == sel_day;
        int is_wkend = (col == 0 || col == 6);

        uint32_t cbg = gbg;
        if (is_sel)   cbg = t->accent;
        else if (is_today) cbg = t->is_dark ? 0x1A3A5C : 0xC8DCFF;

        /* hover */
        int hov = (abs_mx >= cx && abs_mx < cx + cell_w && abs_my >= cy && abs_my < cy + cell_h);
        if (hov && !is_sel) cbg = t->is_dark ? 0x252D3A : 0xDDE3F0;
        if (hov && c->clicked) sel_day = day;

        comp_rect(cx + 1, cy + 1, cell_w - 2, cell_h - 2, cbg);

        char db[3]; num2(db, day); if (day < 10) { db[0]=db[1]; db[1]=0; }
        uint32_t dc = is_sel ? 0xFFFFFF
                    : is_today ? t->accent_hi
                    : is_wkend ? 0xFF8080
                    : t->text;
        comp_text(cx + cell_w/2 - 4, cy + 6, db, dc, cbg);
    }

    /* grid lines */
    for (int i = 0; i <= 7; i++)
        comp_rect(c->origin_x + i * cell_w, grid_y, 1, grid_h, t->win_border);
    for (int i = 0; i <= 6; i++)
        comp_rect(c->origin_x, grid_y + i * cell_h, c->width, 1, t->win_border);

    /* ---------- detail panel ---------- */
    int det_y = grid_y + grid_h;
    uint32_t detbg = t->is_dark ? 0x161B22 : 0xE4E8F0;
    comp_rect(c->origin_x, det_y, c->width, c->height - det_y, detbg);
    comp_rect(c->origin_x, det_y, c->width, 1, t->win_border);

    if (sel_day > 0) {
        char dl[40]; int dp2 = 0;
        const char* mn = months[view_month >= 1 && view_month <= 12 ? view_month : 1];
        for (int i=0;mn[i];i++) dl[dp2++]=mn[i];
        dl[dp2++]=' ';
        char db2[3]; num2(db2, sel_day);
        if (sel_day < 10) { dl[dp2++]=db2[0]; }
        else { dl[dp2++]=db2[0]; dl[dp2++]=db2[1]; }
        dl[dp2++]=','; dl[dp2++]=' ';
        for (int i=0;ybuf[i];i++) dl[dp2++]=ybuf[i];
        dl[dp2]=0;
        w_label_color(c->origin_x + 12, det_y + 8, dl, t->accent_hi);
        if (today_match && sel_day == rt.day)
            w_label_dim(c->origin_x + 12, det_y + 26, "Today -- no events");
        else
            w_label_dim(c->origin_x + 12, det_y + 26, "No events");
    } else {
        w_label_dim(c->origin_x + 12, det_y + 8, "Select a day to view events");
    }
}

const app_def_t APP_CALENDAR = { "Calendar", 'D', 0xFF6060, render_calendar, 490, 480 };
