#ifndef VYRO_FONT_H
#define VYRO_FONT_H

#include <stdint.h>

/* Embedded 6x10 bitmap font.
 *
 * vyro_font_glyph(c) returns a pointer to 10 bytes — one row per scanline.
 * Within each byte, bit 5 (0x20) is column 0; bit 0 (0x01) is column 5.
 * Columns 6 and 7 are blank in every glyph (so the kerning advance is 8px).
 *
 * Coverage: ASCII 32..126. Characters outside that range render as a
 * filled box (so they're visible but obviously wrong).
 */
const uint8_t *vyro_font_glyph(unsigned char c);

/* Blit one glyph at (x, y) into a BGRX8888 surface at `pixels` with
 * row stride `stride_px` pixels. `color` is BGRX; transparent pixels
 * are left untouched (the existing background shows through). */
void vyro_font_blit(uint32_t *pixels, int stride_px, int surf_w, int surf_h,
                    int x, int y, char c, uint32_t color);

/* Walk a NUL-terminated string left-to-right with 8px advance per glyph. */
void vyro_font_text(uint32_t *pixels, int stride_px, int surf_w, int surf_h,
                    int x, int y, const char *s, uint32_t color);

#define VYRO_FONT_W 6
#define VYRO_FONT_H 10
#define VYRO_FONT_ADVANCE 8

#endif
