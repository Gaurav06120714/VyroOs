#ifndef WIDGETS_PANEL_H
#define WIDGETS_PANEL_H

#include "../include/types.h"

// Renders the macOS-Sequoia-style widget stack on the right side
// of the desktop. Each widget is a glassmorphic-looking card.
void widgets_panel_draw(uint32_t panel_x, uint32_t panel_y, uint32_t panel_w);

#endif
