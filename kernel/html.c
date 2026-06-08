#include "html.h"
#include "../drivers/framebuffer.h"
#include "../drivers/keyboard.h"
#include "../drivers/timer.h"
#include "../include/types.h"

#define PAGE_BG   FB_COLOR(250, 250, 250)
#define TEXT_COL  FB_COLOR(20, 20, 20)
#define H1_COL    FB_COLOR(20, 40, 120)
#define LINK_COL  FB_COLOR(20, 90, 200)
#define BAR_BG    FB_COLOR(40, 50, 80)

static uint32_t cx, cy;
static uint32_t cur_color;
static int      scale;
static int      bold;

static int tag_is(const char* p, const char* name) {
    int i = 0;
    while (name[i]) {
        char a = p[i]; char b = name[i];
        if (a >= 'A' && a <= 'Z') a += 32;
        if (a != b) return 0;
        i++;
    }

    char e = p[i];
    return e == '>' || e == ' ' || e == '/';
}

static void emit_char(char c) {
    if (c == '\n') { cx = 40; cy += 20 * scale; return; }
    if (cx > FB_WIDTH - 40) { cx = 40; cy += 18; }
    uint32_t col = bold ? FB_COLOR(0,0,0) : cur_color;
    fb_draw_glyph(cx, cy, c, col, PAGE_BG);
    cx += (scale == 2) ? 11 : 8;
}

static void emit_text(const char* s, int len) {
    for (int i = 0; i < len; i++) {

        if (s[i] == '\n' || s[i] == '\t') { emit_char(' '); }
        else emit_char(s[i]);
    }
}

void html_render(const char* html) {
    if (!fb_available()) return;

    fb_clear(PAGE_BG);

    fb_fill_rect(0, 0, FB_WIDTH, 26, BAR_BG);
    fb_draw_text(8, 5, "VyroBrowser  -  vyro://home  (ESC to exit)", FB_WHITE, BAR_BG);

    cx = 40; cy = 50;
    cur_color = TEXT_COL; scale = 1; bold = 0;

    const char* p = html;
    while (*p) {
        if (*p == '<') {
            p++;
            int closing = 0;
            if (*p == '/') { closing = 1; p++; }

            if (tag_is(p, "h1")) { if (!closing) { scale=2; cur_color=H1_COL; cx=40; if(cy>50)cy+=10; } else { scale=1; cur_color=TEXT_COL; cx=40; cy+=24; } }
            else if (tag_is(p, "h2")) { if (!closing) { scale=2; cur_color=FB_COLOR(60,60,60); cx=40; cy+=6; } else { scale=1; cur_color=TEXT_COL; cx=40; cy+=22; } }
            else if (tag_is(p, "p"))  { if (!closing) { cx=40; cy+=8; } else { cx=40; cy+=20; } }
            else if (tag_is(p, "b"))  { bold = !closing; }
            else if (tag_is(p, "a"))  { if (!closing) cur_color=LINK_COL; else cur_color=TEXT_COL; }
            else if (tag_is(p, "li")) { if (!closing) { cx=40; cy+=18; emit_text("  - ", 4); } }
            else if (tag_is(p, "br")) { cx=40; cy+=18; }

            while (*p && *p != '>') p++;
            if (*p == '>') p++;
        } else {

            const char* start = p;
            while (*p && *p != '<') p++;
            emit_text(start, (int)(p - start));
        }
        if (cy > FB_HEIGHT - 20) break;
    }


    while (1) {
        if (keyboard_has_input() && keyboard_getchar() == 0x1B) break;
        sleep_ms(10);
    }
}
