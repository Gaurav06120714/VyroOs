#include "apps.h"
#include "../widgets.h"
#include "../theme.h"
#include "../compositor.h"

#define HIST_MAX 6

static long  acc      = 0;
static long  cur      = 0;
static char  op       = 0;
static int   in_input = 0;
static int   had_dot  = 0;
static int   error    = 0;

static char  hist[HIST_MAX][32];
static int   hist_n   = 0;

static void hist_push(const char* s) {
    if (hist_n < HIST_MAX) {
        int i = 0; while (s[i] && i < 31) { hist[hist_n][i] = s[i]; i++; }
        hist[hist_n++][i] = 0;
    } else {
        for (int i = 1; i < HIST_MAX; i++) {
            int j = 0;
            while ((hist[i-1][j] = hist[i][j])) j++;
        }
        int i = 0; while (s[i] && i < 31) { hist[HIST_MAX-1][i] = s[i]; i++; }
        hist[HIST_MAX-1][i] = 0;
    }
}

static void long_to_str(char* out, long v) {
    if (v == 0) { out[0]='0'; out[1]=0; return; }
    int neg = v < 0; if (neg) v = -v;
    char r[22]; int n = 0;
    while (v) { r[n++] = '0' + (int)(v % 10); v /= 10; }
    int p = 0; if (neg) out[p++] = '-';
    while (n) out[p++] = r[--n];
    out[p] = 0;
}

static void apply_op() {
    if (error) return;
    long result = acc;
    switch (op) {
        case '+': result = acc + cur; break;
        case '-': result = acc - cur; break;
        case '*': result = acc * cur; break;
        case '/':
            if (cur == 0) { error = 1; return; }
            result = acc / cur;
            break;
        case '%':
            if (cur == 0) { error = 1; return; }
            result = acc % cur;
            break;
        default: result = cur; break;
    }
    char rec[32];
    char ab[12], cb[12], rb[12];
    long_to_str(ab, acc); long_to_str(cb, cur); long_to_str(rb, result);
    int p = 0;
    for (int i=0;ab[i]&&p<30;i++) rec[p++]=ab[i];
    if (op) rec[p++]=op;
    for (int i=0;cb[i]&&p<30;i++) rec[p++]=cb[i];
    rec[p++]='=';
    for (int i=0;rb[i]&&p<30;i++) rec[p++]=rb[i];
    rec[p]=0;
    hist_push(rec);
    acc = result; cur = 0; in_input = 0; op = 0; had_dot = 0;
}

static void render_calc(app_ctx_t* c) {
    const theme_t* t = theme();
    int abs_mx = c->mx + c->origin_x, abs_my = c->my + c->origin_y;

    /* background */
    uint32_t bg = t->is_dark ? 0x161B22 : 0xF0F0F5;
    comp_rect(c->origin_x, c->origin_y, c->width, c->height, bg);

    /* display panel */
    int dpx = c->origin_x + 10, dpy = c->origin_y + 10;
    int dpw = c->width - 20,    dph = 72;
    uint32_t dbg = t->is_dark ? 0x0D1117 : 0x1A1A2E;
    comp_rect(dpx, dpy, dpw, dph, dbg);
    comp_border(dpx, dpy, dpw, dph, t->is_dark ? 0x30363D : 0x888888);

    /* history lines */
    for (int i = 0; i < hist_n && i < 3; i++) {
        int hy = dpy + 6 + i * 14;
        comp_text(dpx + 8, hy, hist[hist_n-1-i < 0 ? 0 : hist_n-1-i], 0x505870, dbg);
    }

    /* main value */
    char buf[24];
    if (error) {
        buf[0]='E'; buf[1]='r'; buf[2]='r'; buf[3]=0;
    } else {
        long_to_str(buf, in_input ? cur : acc);
    }
    int blen = 0; while (buf[blen]) blen++;
    int tx = dpx + dpw - 8 - blen * 14;
    for (int i = 0; buf[i]; i++) {
        comp_glyph(tx + i*14,     dpy + dph - 22, buf[i], 0xA8FFA8, dbg);
        comp_glyph(tx + i*14 + 1, dpy + dph - 22, buf[i], 0xA8FFA8, dbg);
    }

    /* op indicator */
    if (op) {
        char ob[2] = {op, 0};
        comp_text(dpx + 8, dpy + dph - 20, ob, 0x79C0FF, dbg);
    }

    /* button grid */
    static const char* klabel[20] = {
        "C",  "%",  "+/-", "/",
        "7",  "8",  "9",   "*",
        "4",  "5",  "6",   "-",
        "1",  "2",  "3",   "+",
        "0",  ".",  "<",   "="
    };
    static const uint32_t kcol[20] = {
        0xFF7070, 0xA0A0FF, 0xA0A0FF, 0x79C0FF,
        0,        0,        0,        0x79C0FF,
        0,        0,        0,        0x79C0FF,
        0,        0,        0,        0x79C0FF,
        0,        0,        0xFFD080, 0x58A6FF
    };

    int btn_w = (c->width - 20) / 4 - 4;
    int btn_h = (c->height - dph - 30) / 5 - 4;
    int bx0 = c->origin_x + 10;
    int by0 = c->origin_y + dph + 18;

    for (int r = 0; r < 5; r++) {
        for (int col = 0; col < 4; col++) {
            const char* k = klabel[r*4 + col];
            uint32_t kc  = kcol[r*4 + col];
            int bx = bx0 + col * (btn_w + 4);
            int by = by0 + r   * (btn_h + 4);

            /* custom colored button */
            int hov = (abs_mx >= bx && abs_mx < bx+btn_w && abs_my >= by && abs_my < by+btn_h);
            uint32_t bbg = kc ? kc : (t->is_dark ? 0x252D3A : 0xDEE3EE);
            if (hov) { /* lighten */ bbg = kc ? (kc | 0x404040) : (t->is_dark ? 0x3A4455 : 0xC8D0E0); }
            comp_rect(bx, by, btn_w, btn_h, bbg);
            comp_border(bx, by, btn_w, btn_h, t->win_border);
            int kl = 0; while(k[kl]) kl++;
            int ktx = bx + btn_w/2 - kl*4;
            uint32_t ktc = kc ? 0x0D1117 : t->text;
            comp_text(ktx, by + btn_h/2 - 7, k, ktc, bbg);

            if (hov && c->clicked) {
                if (k[0] >= '0' && k[0] <= '9' && !k[1]) {
                    if (!error) { cur = cur * 10 + (k[0] - '0'); in_input = 1; }
                } else if (k[0] == 'C') {
                    acc = 0; cur = 0; op = 0; in_input = 0; had_dot = 0; error = 0;
                } else if (k[0] == '=') {
                    apply_op();
                } else if (k[0] == '<') {
                    if (cur >= 10) cur /= 10;
                    else { cur = 0; in_input = 0; }
                } else if (k[0] == '+' || k[0] == '-' || k[0] == '*' || k[0] == '/' || k[0] == '%') {
                    if (!error) {
                        if (in_input) apply_op();
                        op = k[0]; if (!in_input) acc = cur; cur = 0; in_input = 0;
                    }
                } else if (k[0] == '.') {
                    had_dot = 1; (void)had_dot;
                }
            }
        }
    }
}

const app_def_t APP_CALC = { "Calculator", '#', 0xC080FF, render_calc, 280, 460 };
