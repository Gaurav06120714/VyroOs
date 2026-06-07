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
    // vC.6.10.8: comp_gradient_v page-faulted under -cpu max on the back
    // buffer (RIP=0x342F4, write-to-non-present-page on the per-pixel
    // arithmetic loop). Use a single comp_clear to a solid color instead
    // — it covers the screen in one pass with a known-good bounds path
    // and avoids the per-row signed-divide-and-write hotspot that fails
    // somewhere we cannot yet identify. Looks less pretty but boots.
    uint32_t color = 0x4C1D95;
    switch (current_theme) {
    case WP_AURORA: color = 0x4C1D95; break;
    case WP_SUNSET: color = 0xF59E0B; break;
    case WP_OCEAN:  color = 0x0C4A6E; break;
    case WP_FOREST: color = 0x064E3B; break;
    case WP_NIGHT:  color = 0x0F172A; break;
    case WP_CARBON: color = 0x1F2937; break;
    default:        color = 0x4C1D95; break;
    }
    comp_clear(color);
    (void)scatter_dots;
}
