#include "ctxmenu.h"
#include "compositor.h"
#include "theme.h"

#define ITEM_H   24
#define MENU_W   200
#define MENU_PAD 4

void ctxmenu_clear(ctxmenu_t* m) { m->count = 0; m->visible = 0; m->last_action = -1; m->hovered = -1; }

void ctxmenu_add(ctxmenu_t* m, const char* label, int action_id, ctx_item_kind kind) {
    if (m->count >= CTX_MAX_ITEMS) return;
    m->items[m->count].label = label;
    m->items[m->count].kind = kind;
    m->items[m->count].action_id = action_id;
    m->count++;
}

void ctxmenu_show(ctxmenu_t* m, int x, int y) {
    m->x = x; m->y = y; m->visible = 1; m->last_action = -1;
}

void ctxmenu_hide(ctxmenu_t* m) { m->visible = 0; }

static int total_height(ctxmenu_t* m) {
    int h = MENU_PAD * 2;
    for (int i = 0; i < m->count; i++)
        h += (m->items[i].kind == CTX_ITEM_SEP) ? 6 : ITEM_H;
    return h;
}

int ctxmenu_handle_click(ctxmenu_t* m, int mx, int my) {
    if (!m->visible) return 0;
    int h = total_height(m);
    if (mx < m->x || mx >= m->x + MENU_W || my < m->y || my >= m->y + h) {
        ctxmenu_hide(m);
        return -1;
    }
    int y = m->y + MENU_PAD;
    for (int i = 0; i < m->count; i++) {
        int ih = (m->items[i].kind == CTX_ITEM_SEP) ? 6 : ITEM_H;
        if (my >= y && my < y + ih) {
            if (m->items[i].kind == CTX_ITEM_NORMAL || m->items[i].kind == CTX_ITEM_DANGER) {
                m->last_action = m->items[i].action_id;
                ctxmenu_hide(m);
                return 1;
            }
            return 0;
        }
        y += ih;
    }
    return 0;
}

void ctxmenu_draw(ctxmenu_t* m, int mx, int my) {
    if (!m->visible) return;
    const theme_t* t = theme();
    int h = total_height(m);
    int x = m->x;
    int y_ = m->y;

    // Clamp to screen
    if (x + MENU_W > (int)comp_width()) x = comp_width() - MENU_W - 2;
    if (y_ + h > (int)comp_height()) y_ = comp_height() - h - 2;

    comp_shadow(x, y_, MENU_W, h, t->win_shadow);
    comp_rect(x, y_, MENU_W, h, t->dock_bg);
    comp_border(x, y_, MENU_W, h, t->win_border);

    m->hovered = -1;
    int row_y = y_ + MENU_PAD;
    for (int i = 0; i < m->count; i++) {
        if (m->items[i].kind == CTX_ITEM_SEP) {
            for (int xi = 4; xi < MENU_W - 4; xi++)
                comp_pixel(x + xi, row_y + 3, t->win_border);
            row_y += 6;
            continue;
        }
        int hover = (mx >= x && mx < x + MENU_W && my >= row_y && my < row_y + ITEM_H);
        if (hover && m->items[i].kind != CTX_ITEM_DISABLED) {
            m->hovered = i;
            comp_rect(x + 2, row_y, MENU_W - 4, ITEM_H, t->accent);
        }
        uint32_t fg = t->text;
        if (m->items[i].kind == CTX_ITEM_DANGER) fg = 0xFF7070;
        if (m->items[i].kind == CTX_ITEM_DISABLED) fg = t->text_dim;
        if (hover && m->items[i].kind != CTX_ITEM_DISABLED) fg = 0xFFFFFF;
        comp_text_bg_alpha(x + 12, row_y + (ITEM_H - 16) / 2, m->items[i].label, fg);
        row_y += ITEM_H;
    }
}
