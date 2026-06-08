#ifndef APP_H
#define APP_H

#include "../include/types.h"

typedef struct {
    int mx, my;
    int btn;
    int clicked;
    int key;
    int width, height;
    int origin_x, origin_y;
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
