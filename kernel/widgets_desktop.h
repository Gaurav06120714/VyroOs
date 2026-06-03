#ifndef WIDGETS_DESKTOP_H
#define WIDGETS_DESKTOP_H

#include "../include/types.h"

// Desktop widgets that float over the wallpaper as glassmorphism panels.
// Rendered as part of the GUI redraw loop; live data sources (RTC, uptime,
// network status) are read each frame.

void widget_clock_render(uint32_t x, uint32_t y, uint32_t w, uint32_t h);
void widget_calendar_render(uint32_t x, uint32_t y, uint32_t w, uint32_t h);
void widget_weather_render(uint32_t x, uint32_t y, uint32_t w, uint32_t h);

// Render the default widget stack along the right edge of the screen.
void widgets_desktop_render(void);

#endif
