#ifndef WALLPAPER_H
#define WALLPAPER_H

#include "../include/types.h"

enum wallpaper_theme {
    WP_AURORA   = 0,
    WP_SUNSET   = 1,
    WP_OCEAN    = 2,
    WP_FOREST   = 3,
    WP_NIGHT    = 4,
    WP_CARBON   = 5
};

void wallpaper_set(uint8_t theme);
uint8_t wallpaper_get(void);
const char* wallpaper_name(uint8_t theme);

void wallpaper_render(void);

#endif
