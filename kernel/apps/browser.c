#include "apps.h"
#include "../widgets.h"
#include "../theme.h"
#include "../compositor.h"

// Mini windowed browser — uses the same HTML logic conceptually but draws inline
static const char* PAGE =
    "<h1>VyroBrowser 2</h1>"
    "<p>This is a windowed browser tab.</p>"
    "<h2>Features</h2>"
    "<ul>"
    "<li>Runs as a real desktop app</li>"
    "<li>Live address bar (cosmetic)</li>"
    "<li>HTML rendering via compositor</li>"
    "</ul>"
    "<p>Visit github.com/Gaurav06120714/VyroOs</p>";

static int tag_is(const char* p, const char* name) {
    int i = 0;
    while (name[i]) {
        char a = p[i]; char b = name[i];
        if (a >= 'A' && a <= 'Z') a += 32;
        if (a != b) return 0; i++;
    }
    char e = p[i]; return e == '>' || e == ' ' || e == '/';
}

static void render_browser(app_ctx_t* c) {
    const theme_t* t = theme();
    // Address bar
    comp_rect(c->origin_x, c->origin_y, c->width, 36, t->win_title);
    int abs_mx = c->mx + c->origin_x, abs_my = c->my + c->origin_y;
    w_button(c->origin_x + 6, c->origin_y + 6, 32, 24, "<", abs_mx, abs_my, c->clicked);
    w_button(c->origin_x + 42, c->origin_y + 6, 32, 24, ">", abs_mx, abs_my, c->clicked);
    w_button(c->origin_x + 78, c->origin_y + 6, 60, 24, "Reload", abs_mx, abs_my, c->clicked);
    w_input(c->origin_x + 146, c->origin_y + 6, c->width - 220, "vyro://home", 0);

    // Page area
    comp_rect(c->origin_x, c->origin_y + 36, c->width, c->height - 36, 0xFAFAFA);
    int x0 = c->origin_x + 16, y0 = c->origin_y + 52;
    int x = x0, y = y0;
    int big = 0; int bold = 0; uint32_t col = 0x202028;
    const char* p = PAGE;
    while (*p && y < c->origin_y + c->height - 20) {
        if (*p == '<') {
            p++;
            int closing = 0; if (*p == '/') { closing = 1; p++; }
            if (tag_is(p, "h1")) { if (!closing) { big = 1; col = 0x2050C0; x = x0; y += 6; } else { big = 0; col = 0x202028; x = x0; y += 28; } }
            else if (tag_is(p, "h2")) { if (!closing) { big = 1; col = 0x404060; x = x0; y += 4; } else { big = 0; col = 0x202028; x = x0; y += 22; } }
            else if (tag_is(p, "p"))  { if (!closing) { x = x0; y += 6; } else { x = x0; y += 18; } }
            else if (tag_is(p, "b"))  { bold = !closing; }
            else if (tag_is(p, "li")) { if (!closing) { x = x0; y += 18; comp_text_bg_alpha(x, y, "  * ", col); x += 32; } }
            while (*p && *p != '>') p++;
            if (*p == '>') p++;
        } else {
            const char* s = p; while (*p && *p != '<') p++;
            for (const char* q = s; q < p; q++) {
                char ch = *q; if (ch == '\n' || ch == '\t') continue;
                if (x > c->origin_x + c->width - 16) { x = x0; y += 18; }
                comp_glyph(x, y, ch, col, 0xFAFAFA);
                x += big ? 10 : 8;
                if (bold) { comp_glyph(x - (big?5:4), y, ch, col, 0xFAFAFA); }
            }
        }
    }
    (void)t;
}

const app_def_t APP_BROWSER2 = { "Browser", 'W', 0x40C0E0, render_browser, 600, 480 };
