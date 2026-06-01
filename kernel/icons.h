#ifndef ICONS_H
#define ICONS_H

#include "../include/types.h"

// 16x16 bitmap icons. Each icon = 16 rows × 16 bits = 32 bytes.
// '1' bit = foreground (icon color), '0' bit = transparent.
typedef struct {
    const char* name;
    char        glyph;        // fallback letter
    uint32_t    color;        // primary tint (RGB)
    uint8_t     bitmap[32];   // 16x16 mono bitmap
} icon_t;

extern const icon_t ICON_FINDER;
extern const icon_t ICON_TERMINAL;
extern const icon_t ICON_SETTINGS;
extern const icon_t ICON_BROWSER;
extern const icon_t ICON_APPS;
extern const icon_t ICON_TRASH;

void icon_draw(const icon_t* ic, uint32_t x, uint32_t y, uint32_t size);

#endif
