/*
 * font.c - text in the built-in bitmap fonts (font_data.h). Bold and italic
 * have glyphs of their own; underlined text gets a line under the
 * baseline. Characters the fonts lack are drawn as '?'.
 */
#include "font_data.h"
#include "gfx.h"

static const BitmapFont *font_of(int size, int style)
{
    if (size < 0 || size > 2)
        size = 1;
    return &font_data[size][style & 3];   /* 1 bold, 2 italic */
}

static const Glyph *glyph_of(const BitmapFont *f, int ch)
{
    if (ch < FONT_FIRST_CHAR || ch > FONT_LAST_CHAR)
        ch = '?';
    return &f->glyphs[ch - FONT_FIRST_CHAR];
}

int font_height(int size)
{
    return font_of(size, 0)->height;
}

int font_baseline(int size)
{
    return font_of(size, 0)->ascent;
}

int font_char_width(int size, int style, int ch)
{
    return glyph_of(font_of(size, style), ch)->advance;
}

int font_chars_width(int size, int style, const uint16_t *s, int n)
{
    const BitmapFont *f = font_of(size, style);
    int               w = 0;
    for (int i = 0; i < n; i++)
        w += glyph_of(f, s[i])->advance;
    return w;
}

void font_draw(Surface *s, const Clip *c, int size, int style, const uint16_t *text, int n,
               int x, int y, uint32_t color)
{
    const BitmapFont *f = font_of(size, style);
    int x1 = c->x1 > 0 ? c->x1 : 0, y1 = c->y1 > 0 ? c->y1 : 0;
    int x2 = c->x2 < s->w ? c->x2 : s->w, y2 = c->y2 < s->h ? c->y2 : s->h;
    int start = x;
    for (int i = 0; i < n; i++) {
        const Glyph   *g = glyph_of(f, text[i]);
        const uint8_t *rows = f->bitmap + g->bitmap;
        int            bytes = (g->width + 7) / 8;
        for (int r = 0; r < f->height; r++) {
            int py = y + r;
            if (py < y1 || py >= y2)
                continue;
            const uint8_t *row = rows + r * bytes;
            for (int col = 0; col < g->width; col++) {
                int px = x + g->left + col;
                if (px >= x1 && px < x2 && (row[col >> 3] & (0x80 >> (col & 7))))
                    s->px[(size_t)py * s->w + px] = color;
            }
        }
        x += g->advance;
    }
    if ((style & 4) && x > start) {   /* underlined */
        int uy = y + f->ascent + 1 < y + f->height ? y + f->ascent + 1 : y + f->height - 1;
        gfx_line(s, c, start, uy, x - 1, uy, color, false);
    }
}
