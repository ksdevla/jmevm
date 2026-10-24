"""Writes res/pattern.png for the draw MIDlet: 4x3 pixels, 2-bit palette
(red, green, blue, yellow) with tRNS making yellow transparent. Rows:
    0 1 2 3
    1 2 3 0
    2 3 0 1
DrawMIDlet checks these pixels after loading the image."""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', '..', 'util'))
import make_fixtures  # noqa: E402  (the PNG writer of the decoder tests)

rows = [[0, 1, 2, 3], [1, 2, 3, 0], [2, 3, 0, 1]]
pixels = [[(i,) for i in row] for row in rows]
palette = [(255, 0, 0), (0, 255, 0), (0, 0, 255), (255, 255, 0)]
png = make_fixtures.encode(4, 3, make_fixtures.INDEXED, 2, pixels, False, palette,
                           bytes([255, 255, 255, 0]))
os.makedirs(os.path.join(HERE, 'res'), exist_ok=True)
with open(os.path.join(HERE, 'res', 'pattern.png'), 'wb') as f:
    f.write(png)
