#ifndef THEME_H
#define THEME_H

#include "../include/types.h"

typedef struct {
    uint32_t desktop_bg;
    uint32_t desktop_bg2;
    uint32_t taskbar_bg;
    uint32_t dock_bg;
    uint32_t dock_border;
    uint32_t win_title;
    uint32_t win_title_focus;
    uint32_t win_body;
    uint32_t win_border;
    uint32_t win_shadow;
    uint32_t text;
    uint32_t text_dim;
    uint32_t text_invert;
    uint32_t accent;
    uint32_t accent_hi;
    uint32_t danger;
    uint32_t success;
    uint8_t  is_dark;
} theme_t;

void          theme_init();
void          theme_set_dark(int dark);
const theme_t* theme();

#endif
