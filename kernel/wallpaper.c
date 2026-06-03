#include "wallpaper.h"
#include "compositor.h"

static uint8_t current_theme = WP_AURORA;

void    wallpaper_set(uint8_t theme) { current_theme = theme; }
uint8_t wallpaper_get(void)          { return current_theme; }

const char* wallpaper_name(uint8_t theme) {
    switch (theme) {
    case WP_AURORA: return "aurora";
    case WP_SUNSET: return "sunset";
    case WP_OCEAN:  return "ocean";
    case WP_FOREST: return "forest";
    case WP_NIGHT:  return "night";
    case WP_CARBON: return "carbon";
    default:        return "?";
    }
}

static void scatter_dots(uint32_t color, uint32_t count) {
    // Simple LCG so the dot layout is stable across redraws.
    uint32_t s = 0x12345678;
    uint32_t w = comp_width(), h = comp_height();
    for (uint32_t i = 0; i < count; i++) {
        s = s * 1103515245 + 12345;
        uint32_t x = s % w;
        s = s * 1103515245 + 12345;
        uint32_t y = s % h;
        comp_pixel(x, y, color);
    }
}

void wallpaper_render(void) {
    uint32_t top = 0, bot = 0;
    switch (current_theme) {
    case WP_AURORA: top = 0x4C1D95; bot = 0x0EA5E9; break;   // purple → sky
    case WP_SUNSET: top = 0xF59E0B; bot = 0xEC4899; break;   // orange → magenta
    case WP_OCEAN:  top = 0x0C4A6E; bot = 0x14B8A6; break;   // deep blue → teal
    case WP_FOREST: top = 0x064E3B; bot = 0x10B981; break;   // dark teal → emerald
    case WP_NIGHT:  top = 0x000000; bot = 0x0F172A; break;   // black → navy
    case WP_CARBON: top = 0x1F2937; bot = 0x111827; break;   // charcoal
    default:        top = 0x4C1D95; bot = 0x0EA5E9; break;
    }
    comp_gradient_v(0, 0, comp_width(), comp_height(), top, bot);
    if (current_theme == WP_NIGHT) scatter_dots(0xFFFFFF, 220);
}
