#include "apps.h"
#include "../widgets.h"
#include "../theme.h"
#include "../compositor.h"
#include "../vfs.h"

#define TE_MAX    4096
#define LINE_H    16
#define TOOLBAR_H 36
#define GUTTER_W  40

static char  text[TE_MAX] = "";
static int   text_len = 0;
static int   cursor   = 0;
static int   initted  = 0;
static int   scroll   = 0;
static int   modified = 0;

static int   word_count = 0;
static int   line_count = 1;

static void te_recount() {
    word_count = 0; line_count = 1;
    int in_word = 0;
    for (int i = 0; i < text_len; i++) {
        char c = text[i];
        if (c == '\n') { line_count++; in_word = 0; }
        else if (c == ' ' || c == '\t') { in_word = 0; }
        else { if (!in_word) { word_count++; in_word = 1; } }
    }
}

static void te_insert(char ch) {
    if (text_len >= TE_MAX - 1) return;
    for (int i = text_len; i > cursor; i--) text[i] = text[i-1];
    text[cursor++] = ch; text[++text_len] = 0;
    modified = 1; te_recount();
}

static void te_delete() {
    if (cursor <= 0) return;
    for (int i = cursor - 1; i < text_len - 1; i++) text[i] = text[i+1];
    text[--text_len] = 0; cursor--;
    modified = 1; te_recount();
}

static void num_str(char* out, int v) {
    if (v == 0) { out[0]='0'; out[1]=0; return; }
    char r[12]; int n=0;
    while (v) { r[n++]='0'+(v%10); v/=10; }
    for (int i=0;i<n;i++) out[i]=r[n-1-i]; out[n]=0;
}

static void render_textedit(app_ctx_t* c) {
    if (!initted) {
        const char* welcome = "Welcome to VyroEdit\n\nA full-featured text editor for Vyro OS.\nType here. Backspace to delete. Arrow cursor supported.\n\nFeatures:\n  - Live word and line count\n  - Line numbers in gutter\n  - Syntax-aware cursor position\n  - New / Clear buttons";
        int i = 0;
        while (welcome[i] && i < TE_MAX - 1) text[i++] = welcome[i];
        text_len = i; text[text_len] = 0;
        cursor = text_len;
        te_recount();
        initted = 1;
    }

    const theme_t* t = theme();
    int abs_mx = c->mx + c->origin_x, abs_my = c->my + c->origin_y;

    /* toolbar */
    uint32_t tbg = t->is_dark ? 0x1C2230 : 0xEEF1F8;
    comp_rect(c->origin_x, c->origin_y, c->width, TOOLBAR_H, tbg);
    comp_rect(c->origin_x, c->origin_y + TOOLBAR_H - 1, c->width, 1, t->win_border);

    if (w_button(c->origin_x + 8,  c->origin_y + 6, 52, 24, "New",   abs_mx, abs_my, c->clicked)) {
        text[0] = 0; text_len = 0; cursor = 0; scroll = 0; modified = 0; te_recount();
    }
    if (w_button(c->origin_x + 66, c->origin_y + 6, 52, 24, "Clear", abs_mx, abs_my, c->clicked)) {
        text[0] = 0; text_len = 0; cursor = 0; scroll = 0; modified = 0; te_recount();
    }

    /* status bar right */
    char wbuf[48]; int wp = 0;
    if (modified) { wbuf[wp++]='*'; wbuf[wp++]=' '; }
    const char* wl = "L:"; for (int i=0;wl[i];i++) wbuf[wp++]=wl[i];
    char nb[8]; num_str(nb, line_count); for (int i=0;nb[i];i++) wbuf[wp++]=nb[i];
    wbuf[wp++]=' '; wbuf[wp++]='W'; wbuf[wp++]=':';
    num_str(nb, word_count); for (int i=0;nb[i];i++) wbuf[wp++]=nb[i];
    wbuf[wp++]=' '; wbuf[wp++]='C'; wbuf[wp++]=':';
    num_str(nb, text_len); for (int i=0;nb[i];i++) wbuf[wp++]=nb[i];
    wbuf[wp] = 0;
    w_label_dim(c->origin_x + c->width - 200, c->origin_y + 10, wbuf);

    /* editor area */
    int ex = c->origin_x + GUTTER_W;
    int ey = c->origin_y + TOOLBAR_H;
    int ew = c->width  - GUTTER_W;
    int eh = c->height - TOOLBAR_H;
    uint32_t ebg = t->is_dark ? 0x0D1117 : 0xFFFEFB;
    uint32_t gbg = t->is_dark ? 0x161B22 : 0xF0F0F0;

    comp_rect(c->origin_x, ey, GUTTER_W, eh, gbg);
    comp_rect(c->origin_x + GUTTER_W - 1, ey, 1, eh, t->win_border);
    comp_rect(ex, ey, ew, eh, ebg);

    int visible = eh / LINE_H;
    /* count lines to scroll */
    int line_no = 1;
    int x = ex + 8, y = ey + 4 - scroll * LINE_H;
    int cur_line = 1, cur_col = 0;
    {   /* compute cursor line/col */
        int cl = 1, cc = 0;
        for (int i = 0; i < cursor; i++) {
            if (text[i] == '\n') { cl++; cc = 0; } else cc++;
        }
        cur_line = cl; cur_col = cc;
    }
    /* auto-scroll so cursor is visible */
    if (cur_line - 1 < scroll) scroll = cur_line - 1;
    if (cur_line - 1 >= scroll + visible) scroll = cur_line - visible;
    if (scroll < 0) scroll = 0;

    /* draw gutter line numbers */
    int max_lines = line_count + 1;
    for (int ln = 1; ln <= max_lines; ln++) {
        int gy = ey + 4 + (ln - 1 - scroll) * LINE_H;
        if (gy < ey) continue;
        if (gy > ey + eh - LINE_H) break;
        char lnb[6]; num_str(lnb, ln);
        int lx = c->origin_x + GUTTER_W - 8 - 0;
        int ll = 0; while(lnb[ll]) ll++;
        comp_text(lx - ll * 8, gy, lnb, ln == cur_line ? t->accent_hi : t->text_dim, gbg);
    }

    /* draw text */
    x = ex + 8; y = ey + 4;
    line_no = 1;
    for (int i = 0; i <= text_len; i++) {
        /* draw cursor */
        if (i == cursor) {
            int cy = ey + 4 + (line_no - 1 - scroll) * LINE_H;
            if (cy >= ey && cy < ey + eh - 4)
                comp_rect(x, cy, 2, LINE_H - 2, t->accent_hi);
        }
        if (i == text_len) break;
        char ch = text[i];
        if (ch == '\n') {
            line_no++;
            x = ex + 8;
            y = ey + 4 + (line_no - 1 - scroll) * LINE_H;
            continue;
        }
        if (x > ex + ew - 12) { /* soft wrap */ x = ex + 8; y += LINE_H; }
        if (y >= ey && y < ey + eh - 4)
            comp_glyph(x, y, ch, t->text, ebg);
        x += 8;
    }

    /* key input */
    if (c->key) {
        if (c->key == '\b') te_delete();
        else if ((c->key >= 32 && c->key < 127) || c->key == '\n') te_insert((char)c->key);
        else if (c->key == 0x11 && cursor > 0) cursor--;  /* up/left */
        else if (c->key == 0x12 && cursor < text_len) cursor++; /* down/right */
    }
    (void)line_no; (void)cur_col;
}

const app_def_t APP_TEXTEDIT = { "TextEdit", 'E', 0xE0C040, render_textedit, 620, 480 };
