#ifndef WIDGETS_DESKTOP_H
#define WIDGETS_DESKTOP_H

#include "../include/types.h"

void widget_clock_render(uint32_t x, uint32_t y, uint32_t w, uint32_t h);
void widget_calendar_render(uint32_t x, uint32_t y, uint32_t w, uint32_t h);
void widget_weather_render(uint32_t x, uint32_t y, uint32_t w, uint32_t h);

void widgets_desktop_render(void);

#endif
