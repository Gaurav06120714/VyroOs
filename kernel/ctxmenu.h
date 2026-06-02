#ifndef CTXMENU_H
#define CTXMENU_H

#include "../include/types.h"

#define CTX_MAX_ITEMS 12

typedef enum { CTX_ITEM_NORMAL, CTX_ITEM_SEP, CTX_ITEM_DANGER, CTX_ITEM_DISABLED } ctx_item_kind;

typedef struct {
    const char*    label;
    ctx_item_kind  kind;
    int            action_id;
} ctx_item_t;

typedef struct {
    ctx_item_t items[CTX_MAX_ITEMS];
    int        count;
    int        x, y;
    int        visible;
    int        hovered;
    int        last_action;     // set when an item is clicked
} ctxmenu_t;

void ctxmenu_clear(ctxmenu_t* m);
void ctxmenu_add(ctxmenu_t* m, const char* label, int action_id, ctx_item_kind kind);
void ctxmenu_show(ctxmenu_t* m, int x, int y);
void ctxmenu_hide(ctxmenu_t* m);
// returns 1 if a click happened on a menu item (last_action set), 0 otherwise
// returns -1 if click was outside menu (should close)
int  ctxmenu_handle_click(ctxmenu_t* m, int mx, int my);
void ctxmenu_draw(ctxmenu_t* m, int mx, int my);

#endif
