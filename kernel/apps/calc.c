#include "apps.h"
#include "../widgets.h"
#include "../theme.h"
#include "../compositor.h"

static long      acc = 0;
static long      cur = 0;
static char      op  = 0;
static int       in_input = 0;

static void apply_op() {
    switch (op) {
        case '+': acc = acc + cur; break;
        case '-': acc = acc - cur; break;
        case '*': acc = acc * cur; break;
        case '/': if (cur) acc = acc / cur; break;
        default:  acc = cur; break;
    }
    cur = 0; in_input = 0; op = 0;
}

static void render_calc(app_ctx_t* c) {
    const theme_t* t = theme();
    comp_rect(c->origin_x, c->origin_y, c->width, c->height, t->win_body);


    long shown = in_input ? cur : acc;
    char buf[24];
    int neg = shown < 0; if (neg) shown = -shown;
    int p = 0; char rev[20]; int n = 0;
    if (shown == 0) rev[n++] = '0';
    else while (shown) { rev[n++] = '0' + shown % 10; shown /= 10; }
    if (neg) buf[p++] = '-';
    while (n) buf[p++] = rev[--n];
    buf[p] = 0;

    int dx = c->origin_x + 16, dy = c->origin_y + 12;
    comp_rect(dx, dy, c->width - 32, 48, 0x1A1F2C);
    int tx = dx + c->width - 32 - 8 - p * 16;

    for (int i = 0; i < p; i++) {
        comp_glyph(tx + i * 16, dy + 16, buf[i], 0xA8FFA8, 0x1A1F2C);
        comp_glyph(tx + i * 16 + 1, dy + 16, buf[i], 0xA8FFA8, 0x1A1F2C);
    }


    const char* keys[5][4] = {
        {"C","/","*","-"},
        {"7","8","9","+"},
        {"4","5","6"," "},
        {"1","2","3","="},
        {"0",".",""," "},
    };
    int btn_w = 56, btn_h = 40, gap = 6;
    int bx0 = c->origin_x + 16;
    int by0 = c->origin_y + 70;
    int abs_mx = c->mx + c->origin_x, abs_my = c->my + c->origin_y;
    for (int r = 0; r < 5; r++) {
        for (int col = 0; col < 4; col++) {
            const char* k = keys[r][col]; if (!k || !k[0]) continue;
            int bx = bx0 + col * (btn_w + gap);
            int by = by0 + r   * (btn_h + gap);
            if (w_button(bx, by, btn_w, btn_h, k, abs_mx, abs_my, c->clicked)) {
                if (k[0] >= '0' && k[0] <= '9') {
                    cur = cur * 10 + (k[0] - '0'); in_input = 1;
                } else if (k[0] == 'C') { acc = 0; cur = 0; op = 0; in_input = 0; }
                else if (k[0] == '=') { apply_op(); }
                else { if (in_input) apply_op(); op = k[0]; acc = (acc ? acc : cur); cur = 0; in_input = 0; }
            }
        }
    }
}

const app_def_t APP_CALC = { "Calculator", '#', 0xC080FF, render_calc, 264, 320 };
