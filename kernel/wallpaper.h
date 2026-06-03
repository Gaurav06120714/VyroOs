#ifndef WALLPAPER_H
#define WALLPAPER_H

#include "../include/types.h"

// Procedural wallpapers — vertical gradients with optional accent shapes,
// rendered straight into the compositor back buffer. Adds visual interest
// for the glassmorphism panels (v3.15 / v3.33) to blur underneath.

enum wallpaper_theme {
    WP_AURORA   = 0,    // deep purple → sky blue
    WP_SUNSET   = 1,    // orange → magenta
    WP_OCEAN    = 2,    // deep blue → teal
    WP_FOREST   = 3,    // dark teal → emerald
    WP_NIGHT    = 4,    // black → navy with stars
    WP_CARBON   = 5     // neutral charcoal
};

void wallpaper_set(uint8_t theme);
uint8_t wallpaper_get(void);
const char* wallpaper_name(uint8_t theme);

// Paint the current wallpaper across the full back buffer.
void wallpaper_render(void);

#endif
