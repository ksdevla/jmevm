"""Writes src/midp/font_data.c: the built-in bitmap fonts.

Three sizes (9, 11 and 13 pixels, in lines of 11, 13 and 16) in four faces
of DejaVu Sans (Book, Bold, Oblique, Bold Oblique), Latin-1 characters 32 to
255, rendered by FreeType without anti-aliasing so every pixel is either on
or off. The layout is described in src/midp/font_data.h.

    python make_fonts.py FONT_DIR

FONT_DIR holds DejaVuSans.ttf, DejaVuSans-Bold.ttf, DejaVuSans-Oblique.ttf
and DejaVuSans-BoldOblique.ttf (on Windows: C:\\Windows\\Fonts, when DejaVu
is installed). DejaVu's licence: LICENSE-DejaVu.txt.
Needs Pillow.
"""
import os
import sys

from PIL import Image, ImageDraw, ImageFont

SIZES = [(9, 11), (11, 13), (13, 16)]   # (pixels per em, line height)
FACES = ['DejaVuSans.ttf', 'DejaVuSans-Bold.ttf', 'DejaVuSans-Oblique.ttf', 'DejaVuSans-BoldOblique.ttf']
FIRST, LAST = 32, 255
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, '..', 'src', 'midp', 'font_data.c')


def render(path, em, height):
    """Returns (ascent, glyphs, bitmap): glyphs as (advance, left, width,
    offset) and the rows of all glyphs, packed as font_data.h says."""
    font = ImageFont.truetype(path, em)
    # The baseline goes as low as the deepest descender allows.
    below = 0
    for code in range(FIRST, LAST + 1):
        box = font.getbbox(chr(code), anchor='ls')
        if box[2] > box[0]:
            below = max(below, box[3])
    base = height - below
    glyphs, bitmap = [], bytearray()
    pad = em   # room for glyphs that reach left of the pen or past the advance
    for code in range(FIRST, LAST + 1):
        ch = chr(code)
        advance = max(1, int(round(font.getlength(ch))))
        canvas = Image.new('1', (advance + 2 * pad, height), 0)
        draw = ImageDraw.Draw(canvas)
        draw.fontmode = '1'
        if code not in (0x7F,) and not 0x80 <= code < 0xA0:   # control characters stay blank
            draw.text((pad, base), ch, font=font, fill=1, anchor='ls')
        box = canvas.getbbox()
        if box is None:
            glyphs.append((advance, 0, 0, len(bitmap)))
            continue
        x0, _, x1, _ = box
        width = x1 - x0
        offset = len(bitmap)
        for y in range(height):
            row = 0
            for x in range(width):
                row = (row << 1) | (1 if canvas.getpixel((x0 + x, y)) else 0)
            nbytes = (width + 7) // 8
            row <<= nbytes * 8 - width
            bitmap += row.to_bytes(nbytes, 'big')
        glyphs.append((advance, x0 - pad, width, offset))
    return base, glyphs, bitmap


def c_array(name, ctype, values, per_line=16):
    lines = []
    for i in range(0, len(values), per_line):
        lines.append('    ' + ', '.join(str(v) for v in values[i:i + per_line]) + ',')
    return 'static const %s %s[%d] = {\n%s\n};\n' % (ctype, name, len(values), '\n'.join(lines))


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    font_dir = sys.argv[1]
    out = ['/* Written by tools/make_fonts.py from DejaVu Sans; do not edit.',
           ' * DejaVu fonts: Bitstream Vera licence, see LICENSE-DejaVu.txt. */',
           '#include "font_data.h"', '']
    table = []
    for s, (em, height) in enumerate(SIZES):
        row = []
        for f, face in enumerate(FACES):
            base, glyphs, bitmap = render(os.path.join(font_dir, face), em, height)
            name = 'f%d_%d' % (s, f)
            out.append(c_array(name + '_bitmap', 'uint8_t', list(bitmap), 24))
            out.append('static const Glyph %s_glyphs[FONT_GLYPHS] = {' % name)
            for i in range(0, len(glyphs), 6):
                out.append('    ' + ' '.join('{%d, %d, %d, %d},' % g for g in glyphs[i:i + 6]))
            out.append('};\n')
            row.append('{%d, %d, %s_glyphs, %s_bitmap}' % (height, base, name, name))
        table.append('    {' + ', '.join(row) + '},')
    out.append('const BitmapFont font_data[3][4] = {\n%s\n};' % '\n'.join(table))
    with open(OUT, 'w', newline='\n') as f:
        f.write('\n'.join(out) + '\n')


if __name__ == '__main__':
    main()
