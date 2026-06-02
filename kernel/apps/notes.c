#include "apps.h"
#include "../widgets.h"
#include "../theme.h"
#include "../compositor.h"

#define MAX_NOTES 8
#define NOTE_BODY_MAX 512
#define NOTE_TITLE_MAX 32

typedef struct {
    char title[NOTE_TITLE_MAX];
    char body[NOTE_BODY_MAX];
    int  body_len;
    uint8_t used;
} note_t;

static note_t notes[MAX_NOTES];
static int selected = 0;
static int initted = 0;

static void copy_str(char* d, const char* s, int max) {
    int i = 0; while (s[i] && i < max - 1) { d[i] = s[i]; i++; } d[i] = 0;
}

static void seed() {
    copy_str(notes[0].title, "Welcome", NOTE_TITLE_MAX);
    copy_str(notes[0].body, "Vyro OS Notes app. Click a note on the left, then type to edit. Click 'New' to add.", NOTE_BODY_MAX);
    int n = 0; while (notes[0].body[n]) n++;
    notes[0].body_len = n;
    notes[0].used = 1;

    copy_str(notes[1].title, "To Do", NOTE_TITLE_MAX);
    copy_str(notes[1].body, "- Try right-click on desktop\n- Click [P] in top bar for power\n- Press L to lock\n- Press 1-4 to switch desktops", NOTE_BODY_MAX);
    n = 0; while (notes[1].body[n]) n++;
    notes[1].body_len = n;
    notes[1].used = 1;

    copy_str(notes[2].title, "Ideas", NOTE_TITLE_MAX);
    copy_str(notes[2].body, "Build something great today.", NOTE_BODY_MAX);
    n = 0; while (notes[2].body[n]) n++;
    notes[2].body_len = n;
    notes[2].used = 1;
}

static void render_notes(app_ctx_t* c) {
    if (!initted) { seed(); initted = 1; }
    const theme_t* t = theme();
    int abs_mx = c->mx + c->origin_x, abs_my = c->my + c->origin_y;

    // Sidebar
    int sw = 160;
    comp_rect(c->origin_x, c->origin_y, sw, c->height, t->dock_bg);
    if (w_button(c->origin_x + 8, c->origin_y + 8, sw - 16, 26, "+ New",
                 abs_mx, abs_my, c->clicked)) {
        for (int i = 0; i < MAX_NOTES; i++)
            if (!notes[i].used) {
                copy_str(notes[i].title, "Untitled", NOTE_TITLE_MAX);
                notes[i].body[0] = 0; notes[i].body_len = 0; notes[i].used = 1;
                selected = i; break;
            }
    }
    int row_y = c->origin_y + 44;
    for (int i = 0; i < MAX_NOTES; i++) {
        if (!notes[i].used) continue;
        if (w_list_item(c->origin_x + 4, row_y, sw - 8, 30, notes[i].title,
                        i == selected, abs_mx, abs_my, c->clicked))
            selected = i;
        row_y += 32;
    }

    // Editor
    int ex = c->origin_x + sw, ey = c->origin_y;
    int ew = c->width - sw, eh = c->height;
    comp_rect(ex, ey, ew, eh, 0xF8F6F0);
    // Title
    comp_text(ex + 12, ey + 12, notes[selected].title, 0x202020, 0xF8F6F0);
    // Separator
    for (int i = 0; i < ew - 24; i++) comp_pixel(ex + 12 + i, ey + 38, 0xD0D0C0);
    // Body
    int x = ex + 14, y = ey + 50;
    for (int i = 0; i < notes[selected].body_len; i++) {
        char ch = notes[selected].body[i];
        if (ch == '\n') { x = ex + 14; y += 18; continue; }
        if (x > ex + ew - 16) { x = ex + 14; y += 18; }
        comp_glyph(x, y, ch, 0x202020, 0xF8F6F0);
        x += 8;
    }
    // Caret
    if (y < ey + eh - 18) comp_rect(x, y, 2, 16, 0x303030);

    // Input
    if (c->key && notes[selected].used) {
        if (c->key == '\b' && notes[selected].body_len > 0)
            notes[selected].body[--notes[selected].body_len] = 0;
        else if ((c->key >= 32 && c->key < 127) || c->key == '\n') {
            if (notes[selected].body_len < NOTE_BODY_MAX - 1) {
                notes[selected].body[notes[selected].body_len++] = (char)c->key;
                notes[selected].body[notes[selected].body_len] = 0;
            }
        }
    }
    (void)t;
}

const app_def_t APP_NOTES = { "Notes", 'N', 0xFFD080, render_notes, 620, 420 };
