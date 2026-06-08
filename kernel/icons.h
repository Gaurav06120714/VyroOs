#ifndef ICONS_H
#define ICONS_H

#include "../include/types.h"

typedef struct {
    const char* name;
    char        glyph;
    uint32_t    color;
    uint8_t     bitmap[32];
} icon_t;

extern const icon_t ICON_FINDER;
extern const icon_t ICON_TERMINAL;
extern const icon_t ICON_SETTINGS;
extern const icon_t ICON_BROWSER;
extern const icon_t ICON_APPS;
extern const icon_t ICON_TRASH;

void icon_draw(const icon_t* ic, uint32_t x, uint32_t y, uint32_t size);

#endif
