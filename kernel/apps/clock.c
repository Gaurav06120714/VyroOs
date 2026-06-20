#include "apps.h"
#include "../widgets.h"
#include "../theme.h"
#include "../compositor.h"
#include "../../drivers/rtc.h"
#include "../../drivers/timer.h"

static void d2(char* b, int n) { b[0] = '0' + n/10; b[1] = '0' + n%10; }

static const char* months_long[] = {
    "","January","February","March","April","May","June",
    "July","August","September","October","November","December"
};
static const char* wday_names[] = {
    "Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"
};

static int abs_val(int x) { return x < 0 ? -x : x; }

/* Bresenham line for clock hands */
static void draw_line(int x0, int y0, int x1, int y1, uint32_t col) {
    int dx = abs_val(x1-x0), sx = x0<x1 ? 1 : -1;
    int dy = -abs_val(y1-y0), sy = y0<y1 ? 1 : -1;
    int err = dx+dy;
    while (1) {
        comp_pixel(x0, y0, col);
        if (x0==x1 && y0==y1) break;
        int e2 = 2*err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

/* integer sin/cos approximation scaled by 256 */
static int isin(int deg) {
    deg = ((deg % 360) + 360) % 360;
    static const int table[91] = {
        0,4,9,13,18,22,27,31,36,40,44,49,53,57,62,66,70,74,78,83,
        87,91,95,99,103,107,111,115,119,122,126,130,133,137,141,144,
        147,151,154,158,161,164,167,170,173,176,179,182,185,187,190,
        193,195,198,200,202,205,207,209,211,213,215,217,219,221,222,
        224,226,227,229,230,231,233,234,235,236,237,238,239,240,241,
        242,243,243,244,245,245,246,246,247,247,247,248
    };
    if (deg <= 90)  return  table[deg];
    if (deg <= 180) return  table[180-deg];
    if (deg <= 270) return -table[deg-180];
    return -table[360-deg];
}
static int icos(int deg) { return isin(deg + 90); }

static void draw_hand(int cx, int cy, int angle_deg, int len, int thick, uint32_t col) {
    int ex = cx + (icos(angle_deg) * len) / 256;
    int ey = cy - (isin(angle_deg) * len) / 256;
    for (int t = -thick/2; t <= thick/2; t++) {
        draw_line(cx + t, cy, ex + t, ey, col);
        draw_line(cx, cy + t, ex, ey + t, col);
    }
}

static int tab = 0;  /* 0 = clock, 1 = stopwatch, 2 = world */
static int sw_running = 0;
static uint64_t sw_start = 0;
static uint64_t sw_elapsed = 0;

static void num_to_s(char* out, uint64_t v, int mindigits) {
    char r[12]; int n = 0;
    if (v == 0) r[n++] = '0';
    else while (v) { r[n++] = '0' + (int)(v%10); v /= 10; }
    while (n < mindigits) r[n++] = '0';
    for (int i = 0; i < n; i++) out[i] = r[n-1-i];
    out[n] = 0;
}

static void render_clock(app_ctx_t* c) {
    const theme_t* t = theme();
    int abs_mx = c->mx + c->origin_x, abs_my = c->my + c->origin_y;

    uint32_t bg = t->is_dark ? 0x0D1117 : 0xF4F6FA;
    comp_rect(c->origin_x, c->origin_y, c->width, c->height, bg);

    /* tab bar */
    uint32_t tabbg = t->is_dark ? 0x161B22 : 0xE4E8F0;
    comp_rect(c->origin_x, c->origin_y, c->width, 36, tabbg);
    comp_rect(c->origin_x, c->origin_y + 35, c->width, 1, t->win_border);
    static const char* tabs[] = { "Clock", "Stopwatch", "World Time" };
    for (int i = 0; i < 3; i++) {
        int tx = c->origin_x + 8 + i * 120;
        int sel = (i == tab);
        uint32_t tbg2 = sel ? t->accent : tabbg;
        comp_rect(tx, c->origin_y + 4, 112, 28, tbg2);
        if (sel) comp_border(tx, c->origin_y + 4, 112, 28, t->accent_hi);
        int tl = 0; while(tabs[i][tl]) tl++;
        comp_text(tx + 56 - tl*4, c->origin_y + 12, tabs[i],
                  sel ? 0xFFFFFF : t->text_dim, tbg2);
        int hov = (abs_mx >= tx && abs_mx < tx+112 && abs_my >= c->origin_y+4 && abs_my < c->origin_y+32);
        if (hov && c->clicked) tab = i;
    }

    rtc_time_t rt; rtc_read(&rt);

    /* ---- CLOCK TAB ---- */
    if (tab == 0) {
        /* analog clock face */
        int face_r = 110;
        int face_x = c->origin_x + c->width/2;
        int face_y = c->origin_y + 36 + face_r + 20;

        /* outer ring */
        uint32_t face_bg = t->is_dark ? 0x1A2236 : 0xEEF2FF;
        for (int dy = -face_r; dy <= face_r; dy++) {
            for (int dx = -face_r; dx <= face_r; dx++) {
                int r2 = dx*dx + dy*dy;
                if (r2 <= face_r * face_r)
                    comp_pixel(face_x + dx, face_y + dy, face_bg);
            }
        }
        /* ring */
        for (int dy = -face_r; dy <= face_r; dy++) {
            for (int dx = -face_r; dx <= face_r; dx++) {
                int r2 = dx*dx + dy*dy;
                int ro = face_r - 3;
                if (r2 > ro*ro && r2 <= face_r*face_r)
                    comp_pixel(face_x + dx, face_y + dy, t->accent);
            }
        }

        /* hour markers */
        for (int h = 0; h < 12; h++) {
            int ang = h * 30 - 90;
            int x0 = face_x + (icos(ang) * (face_r - 10)) / 256;
            int y0 = face_y - (isin(ang) * (face_r - 10)) / 256;
            int x1 = face_x + (icos(ang) * (face_r - 20)) / 256;
            int y1 = face_y - (isin(ang) * (face_r - 20)) / 256;
            draw_line(x0, y0, x1, y1, t->is_dark ? 0x6080C0 : 0x8090C0);
        }
        /* minute markers */
        for (int m = 0; m < 60; m++) {
            if (m % 5 == 0) continue;
            int ang = m * 6 - 90;
            int x0 = face_x + (icos(ang) * (face_r - 8)) / 256;
            int y0 = face_y - (isin(ang) * (face_r - 8)) / 256;
            int x1 = face_x + (icos(ang) * (face_r - 14)) / 256;
            int y1 = face_y - (isin(ang) * (face_r - 14)) / 256;
            draw_line(x0, y0, x1, y1, t->is_dark ? 0x303850 : 0xB0B8C8);
        }

        /* hands */
        int hour_ang   = (rt.hour % 12) * 30 + rt.minute / 2 - 90;
        int minute_ang = rt.minute * 6 - 90;
        int second_ang = rt.second * 6 - 90;

        draw_hand(face_x, face_y, hour_ang,   60, 4, t->text);
        draw_hand(face_x, face_y, minute_ang, 85, 3, t->text);
        draw_hand(face_x, face_y, second_ang, 90, 1, 0xFF4040);

        /* center dot */
        for (int dy=-4;dy<=4;dy++) for(int dx=-4;dx<=4;dx++)
            if (dx*dx+dy*dy<=16) comp_pixel(face_x+dx, face_y+dy, t->accent_hi);

        /* digital readout below */
        char clk[9]; d2(clk,rt.hour); clk[2]=':'; d2(clk+3,rt.minute); clk[5]=':'; d2(clk+6,rt.second); clk[8]=0;
        int scale = 3, nchars = 8;
        int dw = nchars * 8 * scale;
        int dx2 = c->origin_x + c->width/2 - dw/2;
        int dy2 = face_y + face_r + 16;
        for (int i = 0; i < nchars; i++)
            for (int ds = 0; ds < scale; ds++)
                for (int ds2 = 0; ds2 < scale; ds2++)
                    comp_glyph(dx2 + i*8*scale + ds, dy2 + ds2*14, clk[i], t->accent_hi, bg);

        /* date */
        int dow = 0;
        { int y2=rt.year,m=rt.month,d=rt.day;
          if(m<3){m+=12;y2--;} int K=y2%100,J=y2/100;
          int hh=(d+(13*(m+1))/5+K+K/4+J/4+5*J)%7; dow=(hh+6)%7; }
        const char* mn = months_long[rt.month>=1&&rt.month<=12?rt.month:1];
        char date[48]; int dp2 = 0;
        const char* wn = wday_names[dow]; for(int i=0;wn[i];i++) date[dp2++]=wn[i];
        date[dp2++]=','; date[dp2++]=' ';
        for(int i=0;mn[i];i++) date[dp2++]=mn[i];
        date[dp2++]=' ';
        d2(date+dp2, rt.day); dp2+=2;
        date[dp2++]=','; date[dp2++]=' ';
        date[dp2++]='0'+(rt.year/1000)%10; date[dp2++]='0'+(rt.year/100)%10;
        date[dp2++]='0'+(rt.year/10)%10;   date[dp2++]='0'+rt.year%10;
        date[dp2]=0;
        int dl = dp2;
        comp_text(c->origin_x + c->width/2 - dl*4, dy2 + scale*14 + 6, date, t->text_dim, bg);
    }

    /* ---- STOPWATCH TAB ---- */
    if (tab == 1) {
        uint64_t now_ms = timer_uptime_ms();
        uint64_t elapsed = sw_elapsed;
        if (sw_running) elapsed += now_ms - sw_start;

        uint64_t cs = elapsed / 10;
        uint64_t sec = cs / 100; cs %= 100;
        uint64_t min = sec / 60; sec %= 60;
        uint64_t hr  = min / 60; min %= 60;

        /* display */
        char sw_buf[16]; int sp = 0;
        char nb[8];
        num_to_s(nb, hr,  2); sw_buf[sp++]=nb[0]; sw_buf[sp++]=nb[1]; sw_buf[sp++]=':';
        num_to_s(nb, min, 2); sw_buf[sp++]=nb[0]; sw_buf[sp++]=nb[1]; sw_buf[sp++]=':';
        num_to_s(nb, sec, 2); sw_buf[sp++]=nb[0]; sw_buf[sp++]=nb[1]; sw_buf[sp++]='.';
        num_to_s(nb, cs,  2); sw_buf[sp++]=nb[0]; sw_buf[sp++]=nb[1]; sw_buf[sp]=0;

        int scale = 3, nc = 11;
        int dw = nc * 8 * scale;
        int dx2 = c->origin_x + c->width/2 - dw/2;
        int dy2 = c->origin_y + 80;
        for (int i = 0; i < nc; i++)
            for (int ds = 0; ds < scale; ds++)
                for (int ds2 = 0; ds2 < scale; ds2++)
                    comp_glyph(dx2 + i*8*scale + ds, dy2 + ds2*14, sw_buf[i], t->accent_hi, bg);

        /* buttons */
        int by2 = dy2 + scale*14 + 24;
        const char* lbl = sw_running ? "Stop" : "Start";
        if (w_button(c->origin_x + c->width/2 - 60, by2, 56, 30, lbl, abs_mx, abs_my, c->clicked)) {
            if (sw_running) {
                sw_elapsed += now_ms - sw_start;
                sw_running = 0;
            } else {
                sw_start   = now_ms;
                sw_running = 1;
            }
        }
        if (w_button(c->origin_x + c->width/2 + 4, by2, 56, 30, "Reset", abs_mx, abs_my, c->clicked)) {
            sw_running = 0; sw_elapsed = 0; sw_start = 0;
        }
    }

    /* ---- WORLD TIME TAB ---- */
    if (tab == 2) {
        struct { const char* city; int offset; } zones[] = {
            { "Los Angeles (PST)",  -8 },
            { "New York  (EST)",    -5 },
            { "London    (GMT)",     0 },
            { "Berlin    (CET)",     1 },
            { "Dubai     (GST)",     4 },
            { "Mumbai    (IST)",     5 },
            { "Tokyo     (JST)",     9 },
            { "Sydney    (AEST)",   10 },
        };
        int base_h = rt.hour;
        int y2 = c->origin_y + 50;
        for (int i = 0; i < 8; i++) {
            int zh = ((base_h + zones[i].offset) % 24 + 24) % 24;
            char tb[6]; d2(tb, zh); tb[2]=':'; d2(tb+3, rt.minute); tb[5]=0;
            uint32_t rbg = (i%2==0) ? bg : (t->is_dark ? 0x161B22 : 0xEEF1F8);
            comp_rect(c->origin_x + 12, y2, c->width - 24, 30, rbg);
            comp_text(c->origin_x + 20, y2 + 8, zones[i].city, t->text_dim, rbg);
            /* time right-aligned */
            int tl = 5;
            comp_text(c->origin_x + c->width - 20 - tl*8, y2 + 8, tb, t->accent_hi, rbg);
            y2 += 32;
        }
    }
    (void)abs_my;
}

const app_def_t APP_CLOCK = { "Clock", 'O', 0x60D0FF, render_clock, 420, 460 };
