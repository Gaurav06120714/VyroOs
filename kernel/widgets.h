#ifndef WIDGETS_H
#define WIDGETS_H

#include "../include/types.h"

int  w_button(int x, int y, int w, int h, const char* label,
              int mx, int my, int clicked);

void w_label(int x, int y, const char* text);
void w_label_dim(int x, int y, const char* text);
void w_label_color(int x, int y, const char* text, uint32_t color);

void w_panel(int x, int y, int w, int h);
void w_panel_titled(int x, int y, int w, int h, const char* title);

void w_separator(int x, int y, int w);

void w_progress(int x, int y, int w, int h, int pct);

int  w_toggle(int x, int y, int state, int mx, int my, int clicked);

void w_input(int x, int y, int w, const char* text, int focused);

int  w_list_item(int x, int y, int w, int h, const char* text,
                 int selected, int mx, int my, int clicked);

int  w_icon_tile(int x, int y, int size, const char* label,
                 char icon_glyph, uint32_t icon_color,
                 int mx, int my, int clicked);

#endif
