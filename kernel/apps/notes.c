#include "apps.h"
#include "../widgets.h"
#include "../theme.h"
#include "../compositor.h"
#include "../../drivers/rtc.h"

#define MAX_NOTES    8
#define BODY_MAX   512
#define TITLE_MAX   32
#define SIDEBAR_W  176
#define TOOLBAR_H   38
#define LINE_H      16

typedef struct {
    char  title[TITLE_MAX];
    char  body[BODY_MAX];
    int   body_len;
    int   m, d;       /* month/day created */
    uint8_t used;
} note_t;

static note_t notes[MAX_NOTES];
static int    selected  = 0;
static int    initted   = 0;
static int    editing_title = 0;

static void cpy(char* d, const char* s, int max) {
    int i=0; while(s[i]&&i<max-1){d[i]=s[i];i++;} d[i]=0;
}
static int slen(const char* s){int i=0;while(s[i])i++;return i;}

static void seed() {
    rtc_time_t rt; rtc_read(&rt);

    cpy(notes[0].title, "Welcome", TITLE_MAX);
    cpy(notes[0].body,
        "Welcome to Vyro Notes!\n\n"
        "Click a note on the left to read it.\n"
        "Type in the right panel to edit.\n"
        "Click '+ New' to create a new note.\n"
        "Click the title to rename it.",
        BODY_MAX);
    notes[0].body_len = slen(notes[0].body);
    notes[0].m = rt.month; notes[0].d = rt.day; notes[0].used = 1;

    cpy(notes[1].title, "Quick Tips", TITLE_MAX);
    cpy(notes[1].body,
        "VyroOS Tips:\n"
        "- Right-click desktop for context menu\n"
        "- Press L to lock screen\n"
        "- Press 1-4 to switch virtual desktops\n"
        "- Press T to toggle dark/light theme\n"
        "- Top-right [P] button for power menu",
        BODY_MAX);
    notes[1].body_len = slen(notes[1].body);
    notes[1].m = rt.month; notes[1].d = rt.day; notes[1].used = 1;

    cpy(notes[2].title, "Build Notes", TITLE_MAX);
    cpy(notes[2].body,
        "VyroOS Build:\n"
        "  make build   -- compile kernel\n"
        "  make run     -- launch in QEMU\n"
        "  make clean   -- remove build artifacts\n\n"
        "Kernel: ~53,000 lines, 201 source files",
        BODY_MAX);
    notes[2].body_len = slen(notes[2].body);
    notes[2].m = rt.month; notes[2].d = rt.day; notes[2].used = 1;
}

static void new_note() {
    for (int i = 0; i < MAX_NOTES; i++) {
        if (!notes[i].used) {
            rtc_time_t rt; rtc_read(&rt);
            cpy(notes[i].title, "Untitled", TITLE_MAX);
            notes[i].body[0] = 0; notes[i].body_len = 0;
            notes[i].m = rt.month; notes[i].d = rt.day;
            notes[i].used = 1;
            selected = i;
            editing_title = 0;
            return;
        }
    }
}

static void render_notes(app_ctx_t* c) {
    if (!initted) { seed(); initted = 1; }
    const theme_t* t = theme();
    int abs_mx = c->mx + c->origin_x, abs_my = c->my + c->origin_y;

    /* ---- sidebar ---- */
    uint32_t sbg  = t->is_dark ? 0x161B22 : 0xE8E4D8;
    uint32_t sbdr = t->is_dark ? 0x252D3A : 0xC8C0A8;
    comp_rect(c->origin_x, c->origin_y, SIDEBAR_W, c->height, sbg);
    comp_rect(c->origin_x + SIDEBAR_W - 1, c->origin_y, 1, c->height, sbdr);

    /* new note button */
    if (w_button(c->origin_x + 8, c->origin_y + 8, SIDEBAR_W - 16, 26,
                 "+ New Note", abs_mx, abs_my, c->clicked)) new_note();

    /* note list */
    int sy = c->origin_y + 44;
    for (int i = 0; i < MAX_NOTES; i++) {
        if (!notes[i].used) continue;
        int sel = (i == selected);
        int hov = (abs_mx >= c->origin_x + 4 && abs_mx < c->origin_x + SIDEBAR_W - 4 &&
                   abs_my >= sy && abs_my < sy + 52);
        uint32_t ibg = sel  ? t->accent
                     : hov  ? (t->is_dark ? 0x252D3A : 0xD4CEB8)
                     : sbg;
        comp_rect(c->origin_x + 4, sy, SIDEBAR_W - 8, 52, ibg);
        comp_border(c->origin_x + 4, sy, SIDEBAR_W - 8, 52, sel ? t->accent_hi : sbdr);
        uint32_t tc = sel ? 0xFFFFFF : t->text;
        comp_text(c->origin_x + 10, sy + 6, notes[i].title, tc, ibg);
        /* date */
        char db[8]; db[0]='0'+notes[i].m/10; db[1]='0'+notes[i].m%10;
        db[2]='/'; db[3]='0'+notes[i].d/10; db[4]='0'+notes[i].d%10; db[5]=0;
        comp_text(c->origin_x + 10, sy + 26, db, sel ? 0xCCDDFF : t->text_dim, ibg);
        /* preview */
        char prev[20]; int pp = 0;
        for (int j = 0; notes[i].body[j] && pp < 18; j++) {
            char ch = notes[i].body[j];
            if (ch == '\n') break;
            prev[pp++] = ch;
        }
        prev[pp] = 0;
        comp_text(c->origin_x + 10, sy + 38, prev, sel ? 0xAABBDD : t->text_dim, ibg);

        if (hov && c->clicked) { selected = i; editing_title = 0; }
        sy += 56;
    }

    /* ---- editor pane ---- */
    int ex = c->origin_x + SIDEBAR_W;
    int ey = c->origin_y;
    int ew = c->width - SIDEBAR_W;
    uint32_t ebg = t->is_dark ? 0x0F1419 : 0xFFFDF5;
    comp_rect(ex, ey, ew, c->height, ebg);

    if (!notes[selected].used) {
        w_label_dim(ex + ew/2 - 60, ey + c->height/2, "No note selected");
        return;
    }

    /* toolbar */
    uint32_t tbg = t->is_dark ? 0x1C2230 : 0xF0EDE0;
    comp_rect(ex, ey, ew, TOOLBAR_H, tbg);
    comp_rect(ex, ey + TOOLBAR_H - 1, ew, 1, sbdr);

    /* delete button */
    if (w_button(ex + ew - 76, ey + 7, 68, 24, "Delete", abs_mx, abs_my, c->clicked)) {
        notes[selected].used = 0;
        for (int i = 0; i < MAX_NOTES; i++) if (notes[i].used) { selected = i; break; }
        editing_title = 0;
        return;
    }

    /* char count */
    char cb[16] = "chars: "; int cp = 7;
    int cv = notes[selected].body_len;
    if (cv == 0) cb[cp++]='0';
    else { char r[8]; int n=0; while(cv){r[n++]='0'+cv%10;cv/=10;} while(n) cb[cp++]=r[--n]; }
    cb[cp]=0;
    w_label_dim(ex + ew - 180, ey + 11, cb);

    /* editable title */
    int ti_y = ey + TOOLBAR_H + 8;
    uint32_t title_bg = editing_title
        ? (t->is_dark ? 0x1A2236 : 0xFFFFEE)
        : ebg;
    comp_rect(ex + 8, ti_y, ew - 16, 26, title_bg);
    comp_border(ex + 8, ti_y, ew - 16, 26, editing_title ? t->accent : ebg);
    comp_text(ex + 14, ti_y + 5, notes[selected].title,
              t->is_dark ? 0xE6EDF3 : 0x202020, title_bg);
    if (abs_mx >= ex + 8 && abs_mx < ex + ew - 8 &&
        abs_my >= ti_y && abs_my < ti_y + 26 && c->clicked)
        editing_title = 1;

    /* separator */
    int sep_y = ti_y + 32;
    comp_rect(ex + 8, sep_y, ew - 16, 1, sbdr);

    /* body text area */
    int bx = ex + 14, by = sep_y + 8;
    int x = bx, y = by;
    for (int i = 0; i < notes[selected].body_len; i++) {
        char ch = notes[selected].body[i];
        if (ch == '\n') { x = bx; y += LINE_H; continue; }
        if (x > ex + ew - 16) { x = bx; y += LINE_H; }
        if (y < ey + c->height - 20)
            comp_glyph(x, y, ch, t->text, ebg);
        x += 8;
    }
    /* cursor */
    if (!editing_title && y < ey + c->height - 20)
        comp_rect(x, y, 2, LINE_H - 2, t->accent_hi);

    /* key input */
    if (c->key && notes[selected].used) {
        if (editing_title) {
            int tl = slen(notes[selected].title);
            if (c->key == '\b' && tl > 0) notes[selected].title[--tl] = 0;
            else if (c->key == '\n') editing_title = 0;
            else if (c->key >= 32 && c->key < 127 && tl < TITLE_MAX - 1) {
                notes[selected].title[tl++] = (char)c->key;
                notes[selected].title[tl]   = 0;
            }
        } else {
            int bl = notes[selected].body_len;
            if (c->key == '\b' && bl > 0)
                notes[selected].body[--notes[selected].body_len] = 0;
            else if ((c->key >= 32 && c->key < 127) || c->key == '\n') {
                if (bl < BODY_MAX - 1) {
                    notes[selected].body[bl++] = (char)c->key;
                    notes[selected].body[bl]   = 0;
                    notes[selected].body_len = bl;
                }
            }
        }
    }
    (void)t;
}

const app_def_t APP_NOTES = { "Notes", 'N', 0xFFD080, render_notes, 660, 480 };
