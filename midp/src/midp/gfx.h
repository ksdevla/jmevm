/*
 * gfx.h - the software renderer behind javax.microedition.lcdui.Graphics.
 *
 * Pixels are 0xAARRGGBB, the format of MIDP's getRGB and drawRGB. Images
 * may have translucent pixels; what is drawn on (the screen, mutable
 * images) is opaque. All arithmetic is integer, so every platform draws
 * the same pixels.
 */
#ifndef MIDP_GFX_H
#define MIDP_GFX_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct Surface {
    uint32_t *px;
    bool      alpha;    /* whether any pixel may be less than opaque */
    int       w, h;
} Surface;

/* A clip rectangle in surface coordinates; x2 and y2 are exclusive. */
typedef struct Clip {
    int x1, y1, x2, y2;
} Clip;

/* The colour seen when src (alpha a, 0-255) covers the opaque dst. */
static inline uint32_t gfx_blend(uint32_t dst, uint32_t src, unsigned a)
{
    if (a >= 255)
        return src | 0xFF000000u;
    if (a == 0)
        return dst;
    unsigned keep = 255 - a;
    unsigned r = (((src >> 16) & 0xFF) * a + ((dst >> 16) & 0xFF) * keep + 127) / 255;
    unsigned g = (((src >> 8) & 0xFF) * a + ((dst >> 8) & 0xFF) * keep + 127) / 255;
    unsigned b = ((src & 0xFF) * a + (dst & 0xFF) * keep + 127) / 255;
    return 0xFF000000u | (r << 16) | (g << 8) | b;
}

/* The transforms of Sprite and drawRegion, with their MIDP values. */
enum {
    TRANS_NONE = 0, TRANS_MIRROR_ROT180 = 1, TRANS_MIRROR = 2, TRANS_ROT180 = 3,
    TRANS_MIRROR_ROT270 = 4, TRANS_ROT90 = 5, TRANS_ROT270 = 6, TRANS_MIRROR_ROT90 = 7
};

/* Shapes, as MIDP defines them: filled shapes cover w x h pixels, outlines
 * (w + 1) x (h + 1). Arc angles are degrees counter-clockwise from three
 * o'clock, relative to the bounding box (45 degrees points at its corner).
 * Colours are opaque. */
void gfx_fill_rect(Surface *s, const Clip *c, int x, int y, int w, int h, uint32_t color);
void gfx_line(Surface *s, const Clip *c, int x1, int y1, int x2, int y2, uint32_t color, bool dotted);
void gfx_rect(Surface *s, const Clip *c, int x, int y, int w, int h, uint32_t color, bool dotted);
void gfx_fill_triangle(Surface *s, const Clip *c, int x1, int y1, int x2, int y2, int x3, int y3,
                       uint32_t color);
/* An even-odd filled polygon, its edges included. */
void gfx_fill_polygon(Surface *s, const Clip *c, const int *xs, const int *ys, int n, uint32_t color);
void gfx_arc(Surface *s, const Clip *c, int x, int y, int w, int h, int start, int arc,
             uint32_t color, bool fill);
void gfx_round_rect(Surface *s, const Clip *c, int x, int y, int w, int h, int aw, int ah,
                    uint32_t color, bool fill);

/* Draws the w x h region of src at (sx, sy), transformed, with its top left
 * at (dx, dy). With blend, translucent pixels are blended onto dst;
 * without, pixels are copied as they are. src may be dst (copyArea). */
void gfx_blit(Surface *dst, const Clip *c, const Surface *src, int sx, int sy, int w, int h,
              int transform, int dx, int dy, bool blend);

/* MIDP drawRGB: w x h pixels from rgb[offset], rows scan apart (scan may
 * be negative). The caller has checked that they are all in the array. */
void gfx_draw_rgb(Surface *dst, const Clip *c, const int32_t *rgb, int offset, int scan,
                  int x, int y, int w, int h, bool alpha);

/* Text (font.c). size: 0 small, 1 medium, 2 large; style: the bits of
 * javax.microedition.lcdui.Font (1 bold, 2 italic, 4 underlined). (x, y) is
 * the top left of the line. */
int  font_height(int size);
int  font_baseline(int size);
int  font_char_width(int size, int style, int ch);
int  font_chars_width(int size, int style, const uint16_t *s, int n);
void font_draw(Surface *s, const Clip *c, int size, int style, const uint16_t *text, int n,
               int x, int y, uint32_t color);

#endif
