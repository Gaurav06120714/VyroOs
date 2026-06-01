#ifndef APP_H
#define APP_H

#include "../include/types.h"

// ─────────────────────────────────────────────────
// Vyro OS Application Framework v2
// Apps register a render callback. The desktop runs
// them inside windows with input event dispatch.
// ─────────────────────────────────────────────────

typedef struct {
    int mx, my;          // mouse position relative to window body
    int btn;             // mouse button state
    int clicked;         // edge-triggered: left-click this frame
    int key;             // last keystroke (0 if none)
    int width, height;   // window body size
    int origin_x, origin_y;   // window body origin in screen coords
} app_ctx_t;

typedef void (*app_render_fn)(app_ctx_t* ctx);

typedef struct {
    const char*   name;
    char          icon_glyph;
    uint32_t      icon_color;
    app_render_fn render;
    int           default_w;
    int           default_h;
} app_def_t;

#define MAX_APPS 16
void app_register(const app_def_t* def);
int  app_count();
const app_def_t* app_get(int i);
const app_def_t* app_find(const char* name);

#endif
