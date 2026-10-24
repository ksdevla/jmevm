"""Writes the test files for util_test.c into this directory.

DEFLATE: one input, compressed every way zlib can (stored, fixed and
dynamic Huffman codes, Huffman only, run lengths, every level), as raw
DEFLATE (.deflate) and as a zlib stream (.zlib).

PNG: small images of every colour type and bit depth, with each of the five
row filters, plain and Adam7-interlaced, with and without tRNS. The PNGs are
written here by hand so all of that is under control, and each comes with
the pixels it must decode to (.argb: little-endian 0xAARRGGBB per pixel),
computed from the source values the way png.c scales them.

Run it with Python 3; the files it writes are committed, so building and
testing jmevm does not need Python.
"""
import os
import random
import struct
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
rng = random.Random(1951)


def write(name, data):
    with open(os.path.join(HERE, name), 'wb') as f:
        f.write(data)


# ---- DEFLATE ----

def sample_input():
    parts = []
    words = [b'MIDlet', b'Canvas', b'Graphics', b'record', b'store', b'jmevm ', b'\n']
    for _ in range(1500):   # text-like: many matches at many distances
        parts.append(rng.choice(words))
    parts.append(bytes(rng.randrange(256) for _ in range(3000)))   # incompressible
    parts.append(b'\0' * 5000)                                       # long runs
    parts.append(bytes(range(256)) * 20)                             # far matches
    return b''.join(parts)


def deflate_files():
    data = sample_input()
    write('input.bin', data)
    variants = {
        'stored': dict(level=0),
        'fast': dict(level=1),
        'default': dict(level=6),
        'best': dict(level=9),
        'fixed': dict(level=6, strategy=zlib.Z_FIXED),
        'huffman': dict(level=6, strategy=zlib.Z_HUFFMAN_ONLY),
        'rle': dict(level=6, strategy=zlib.Z_RLE),
        'smallwindow': dict(level=9, wbits=9),
    }
    for name, opts in variants.items():
        wbits = opts.pop('wbits', 15)
        raw = zlib.compressobj(wbits=-wbits, **opts)
        write(name + '.deflate', raw.compress(data) + raw.flush())
        wrapped = zlib.compressobj(wbits=wbits, **opts)
        write(name + '.zlib', wrapped.compress(data) + wrapped.flush())
    # Many small blocks: flush after every piece.
    c = zlib.compressobj(level=6, wbits=-15)
    out = b''
    for i in range(0, len(data), 997):
        out += c.compress(data[i:i + 997]) + c.flush(zlib.Z_FULL_FLUSH)
    write('blocks.deflate', out + c.flush())
    # Empty input.
    c = zlib.compressobj(level=6, wbits=-15)
    write('empty.deflate', c.compress(b'') + c.flush())


# ---- PNG ----

GRAY, RGB, INDEXED, GRAY_ALPHA, RGB_ALPHA = 0, 2, 3, 4, 6
SAMPLES = {GRAY: 1, RGB: 3, INDEXED: 1, GRAY_ALPHA: 2, RGB_ALPHA: 4}
ADAM7 = [(0, 0, 8, 8), (4, 0, 8, 8), (0, 4, 4, 8), (2, 0, 4, 4), (0, 2, 2, 4), (1, 0, 2, 2), (0, 1, 1, 2)]


def chunk(kind, body):
    return struct.pack('>I', len(body)) + kind + body + struct.pack('>I', zlib.crc32(kind + body))


def pack_row(values, depth):
    if depth == 16:
        return b''.join(struct.pack('>H', v) for v in values)
    if depth == 8:
        return bytes(values)
    out, acc, n = bytearray(), 0, 0
    for v in values:
        acc = (acc << depth) | v
        n += depth
        if n == 8:
            out.append(acc)
            acc, n = 0, 0
    if n:
        out.append(acc << (8 - n))
    return bytes(out)


def paeth(a, b, c):
    p = a + b - c
    pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    return b if pb <= pc else c


def filter_row(kind, row, above, bpp):
    out = bytearray()
    for i, x in enumerate(row):
        left = row[i - bpp] if i >= bpp else 0
        up = above[i] if above else 0
        ul = above[i - bpp] if above and i >= bpp else 0
        pred = [0, left, up, (left + up) // 2, paeth(left, up, ul)][kind]
        out.append((x - pred) & 0xFF)
    return bytes([kind]) + bytes(out)


def encode(width, height, ctype, depth, pixels, interlace, palette=None, trns=None):
    """pixels[y][x] is a tuple of sample values at depth."""
    bits = SAMPLES[ctype] * depth
    bpp = max(1, bits // 8)
    passes = ADAM7 if interlace else [(0, 0, 1, 1)]
    raw = bytearray()
    row_no = 0
    for x0, y0, dx, dy in passes:
        xs = list(range(x0, width, dx))
        if not xs:
            continue
        above = None
        for y in range(y0, height, dy):
            values = [s for x in xs for s in pixels[y][x]]
            row = pack_row(values, depth)
            raw += filter_row(row_no % 5, row, above, bpp)   # every filter in turn
            above = row
            row_no += 1
    ihdr = struct.pack('>IIBBBBB', width, height, depth, ctype, 0, 0, 1 if interlace else 0)
    png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', ihdr)
    if palette is not None:
        png += chunk(b'PLTE', b''.join(bytes(c) for c in palette))
    if trns is not None:
        png += chunk(b'tRNS', trns)
    z = zlib.compress(bytes(raw), 9)
    png += chunk(b'IDAT', z[:len(z) // 2]) + chunk(b'IDAT', z[len(z) // 2:])   # split
    return png + chunk(b'IEND', b'')


def expected(width, height, ctype, depth, pixels, palette=None, alphas=None, key=None):
    top = (1 << depth) - 1
    out = bytearray()
    for y in range(height):
        for x in range(width):
            v = pixels[y][x]
            if ctype == INDEXED:
                i = v[0]
                r, g, b = palette[i] if i < len(palette) else (0, 0, 0)
                a = alphas[i] if alphas and i < len(alphas) else 255
            else:
                if ctype in (GRAY, GRAY_ALPHA):
                    r = g = b = v[0]
                else:
                    r, g, b = v[0], v[1], v[2]
                a = 255
                if ctype == GRAY_ALPHA:
                    a = v[1] * 255 // top
                elif ctype == RGB_ALPHA:
                    a = v[3] * 255 // top
                elif key is not None and tuple(v[:len(key)]) == key:
                    a = 0
                r, g, b = r * 255 // top, g * 255 // top, b * 255 // top
            out += struct.pack('<I', (a << 24) | (r << 16) | (g << 8) | b)
    return bytes(out)


def png_files():
    width, height = 13, 11   # odd sizes, so Adam7 passes are uneven
    cases = []
    for depth in (1, 2, 4, 8, 16):
        cases.append(('gray%d' % depth, GRAY, depth, None))
    cases.append(('gray8_key', GRAY, 8, 'key'))
    cases.append(('gray16_key', GRAY, 16, 'key'))
    for depth in (8, 16):
        cases.append(('rgb%d' % depth, RGB, depth, None))
        cases.append(('rgb%d_key' % depth, RGB, depth, 'key'))
        cases.append(('grayalpha%d' % depth, GRAY_ALPHA, depth, None))
        cases.append(('rgba%d' % depth, RGB_ALPHA, depth, None))
    for depth in (1, 2, 4, 8):
        cases.append(('indexed%d' % depth, INDEXED, depth, None))
        cases.append(('indexed%d_alpha' % depth, INDEXED, depth, 'alpha'))
    cases.append(('indexed8_short', INDEXED, 8, 'short'))   # indices past the palette

    names = []
    for name, ctype, depth, extra in cases:
        top = (1 << depth) - 1
        n = SAMPLES[ctype]
        colours = 1 << depth if ctype == INDEXED else None
        if ctype == INDEXED:
            size = colours if extra != 'short' else colours // 2
            palette = [(rng.randrange(256), rng.randrange(256), rng.randrange(256)) for _ in range(size)]
            pixels = [[(rng.randrange(colours),) for _ in range(width)] for _ in range(height)]
        else:
            palette = None
            pixels = [[tuple(rng.randrange(top + 1) for _ in range(n)) for _ in range(width)]
                      for _ in range(height)]
        trns, alphas, key = None, None, None
        if extra == 'alpha':
            alphas = [rng.randrange(256) for _ in range(len(palette) - 1)]   # one short
            trns = bytes(alphas)
        elif extra == 'key':
            key = pixels[3][5][:3 if ctype == RGB else 1]
            trns = b''.join(struct.pack('>H', k) for k in key)
        for interlace in (False, True):
            suffix = '_adam7' if interlace else ''
            write(name + suffix + '.png',
                  encode(width, height, ctype, depth, pixels, interlace, palette, trns))
            write(name + suffix + '.argb',
                  expected(width, height, ctype, depth, pixels, palette, alphas, key))
            alpha = ctype in (GRAY_ALPHA, RGB_ALPHA) or trns is not None
            names.append('%s %d %d %d' % (name + suffix, width, height, alpha))
    write('png_list.txt', ('\n'.join(names) + '\n').encode())


# ---- ZIP ----

def zip_file():
    """Entries the way J2ME JARs have them: stored and deflated, a name that
    appears twice (the last one counts), and a comment after the directory."""
    import io
    import warnings
    import zipfile
    buf = io.BytesIO()
    with warnings.catch_warnings():
        warnings.simplefilter('ignore')   # the duplicate name
        with zipfile.ZipFile(buf, 'w') as z:
            z.writestr(zipfile.ZipInfo('a.txt'), b'first')
            z.writestr(zipfile.ZipInfo('stored.bin'), bytes(range(256)) * 4)
            info = zipfile.ZipInfo('dir/deflated.txt')
            info.compress_type = zipfile.ZIP_DEFLATED
            z.writestr(info, b'deflated ' * 500)
            z.writestr(zipfile.ZipInfo('a.txt'), b'second')
            z.comment = b'a comment after the central directory'
    write('dup.zip', buf.getvalue())


if __name__ == '__main__':
    deflate_files()
    png_files()
    zip_file()
