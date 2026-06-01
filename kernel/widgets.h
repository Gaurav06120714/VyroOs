#ifndef WIDGETS_H
#define WIDGETS_H

#include "../include/types.h"

// ─────────────────────────────────────────────────
// Vyro OS widget toolkit — primitives every app uses.
// All draws happen on the compositor's back buffer.
// ─────────────────────────────────────────────────

// Button — returns 1 if clicked this frame
int  w_button(int x, int y, int w, int h, const char* label,
              int mx, int my, int clicked);

// Label
void w_label(int x, int y, const char* text);
void w_label_dim(int x, int y, const char* text);
void w_label_color(int x, int y, const char* text, uint32_t color);

// Panel (card)
void w_panel(int x, int y, int w, int h);
void w_panel_titled(int x, int y, int w, int h, const char* title);

// Separator
void w_separator(int x, int y, int w);

// Progress bar (0..100)
void w_progress(int x, int y, int w, int h, int pct);

// Toggle (returns new state)
int  w_toggle(int x, int y, int state, int mx, int my, int clicked);

// Text input box (display-only here; real input handled by app)
void w_input(int x, int y, int w, const char* text, int focused);

// List item (selectable row)
int  w_list_item(int x, int y, int w, int h, const char* text,
                 int selected, int mx, int my, int clicked);

// Icon tile (for app launchers, dock)
int  w_icon_tile(int x, int y, int size, const char* label,
                 char icon_glyph, uint32_t icon_color,
                 int mx, int my, int clicked);

#endif
