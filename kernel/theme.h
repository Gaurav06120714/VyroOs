#ifndef THEME_H
#define THEME_H

#include "../include/types.h"

// ─────────────────────────────────────────────────
// Vyro OS theme system — every UI color named.
// Switch at runtime with theme_set_dark(1/0).
// ─────────────────────────────────────────────────

typedef struct {
    uint32_t desktop_bg;
    uint32_t desktop_bg2;     // gradient bottom
    uint32_t taskbar_bg;      // top bar
    uint32_t dock_bg;         // bottom dock
    uint32_t dock_border;
    uint32_t win_title;       // unfocused title
    uint32_t win_title_focus; // focused title
    uint32_t win_body;
    uint32_t win_border;
    uint32_t win_shadow;      // drop shadow color
    uint32_t text;
    uint32_t text_dim;
    uint32_t text_invert;     // text on light backgrounds
    uint32_t accent;          // primary brand
    uint32_t accent_hi;       // hover/active
    uint32_t danger;
    uint32_t success;
    uint8_t  is_dark;
} theme_t;

void          theme_init();
void          theme_set_dark(int dark);
const theme_t* theme();

#endif
