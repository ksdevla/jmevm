/*
 * font_data.h - the built-in bitmap fonts: three sizes in four faces
 * (plain, bold, italic, bold italic), Latin-1. font_data.c is written by
 * tools/make_fonts.py from DejaVu Sans.
 */
#ifndef MIDP_FONT_DATA_H
#define MIDP_FONT_DATA_H

#include <stdint.h>

#define FONT_FIRST_CHAR 32
#define FONT_LAST_CHAR  255
#define FONT_GLYPHS     (FONT_LAST_CHAR - FONT_FIRST_CHAR + 1)

typedef struct Glyph {
    uint8_t  advance;   /* pen movement after the glyph */
    int8_t   left;      /* bitmap column 0 relative to the pen */
    uint8_t  width;     /* bitmap columns; rows are the font's height */
    uint32_t bitmap;    /* offset of the first row in BitmapFont.bitmap */
} Glyph;

typedef struct BitmapFont {
    uint8_t        height;   /* line height; every glyph has this many rows */
    uint8_t        ascent;   /* rows above the baseline */
    const Glyph   *glyphs;   /* FONT_GLYPHS of them, from FONT_FIRST_CHAR */
    /* Rows of (width + 7) / 8 bytes, most significant bit leftmost. */
    const uint8_t *bitmap;
} BitmapFont;

/* [size][face]: size 0 small, 1 medium, 2 large; face 0 plain, 1 bold,
 * 2 italic, 3 bold italic. */
extern const BitmapFont font_data[3][4];

#endif
