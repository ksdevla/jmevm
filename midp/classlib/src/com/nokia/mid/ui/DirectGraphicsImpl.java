package com.nokia.mid.ui;

import javax.microedition.lcdui.Graphics;
import javax.microedition.lcdui.Image;

import jmevm.midp.LcduiAccess;

/**
 * DirectGraphics on a Graphics. Everything is drawn through the Graphics
 * (drawRegion, drawRGB), so it keeps its clip and translation; shapes in
 * ARGB colours are rasterized here into a coverage mask and blended.
 *
 * <p>A manipulation flips first, then rotates counter-clockwise; it is
 * turned into the MIDP transform with the same effect.
 */
final class DirectGraphicsImpl implements DirectGraphics {
    /* The MIDP transforms 0..7 as matrices (a, b, c, d): (x, y) -> (ax + by, cx + dy). */
    private static final int[][] TRANSFORMS = {
        {1, 0, 0, 1}, {1, 0, 0, -1}, {-1, 0, 0, 1}, {-1, 0, 0, -1},
        {0, 1, 1, 0}, {0, -1, 1, 0}, {0, 1, -1, 0}, {0, -1, -1, 0}};

    private final Graphics g;
    private int alpha = 0xFF;

    DirectGraphicsImpl(Graphics g) {
        this.g = g;
    }

    public void setARGBColor(int argbColor) {
        alpha = argbColor >>> 24;
        g.setColor(argbColor & 0xFFFFFF);
    }

    public int getAlphaComponent() {
        return alpha;
    }

    public int getNativePixelFormat() {
        return TYPE_INT_8888_ARGB;
    }

    // ---- manipulations

    private static int[] multiply(int[] m, int[] n) {   /* m after n */
        return new int[] {m[0] * n[0] + m[1] * n[2], m[0] * n[1] + m[1] * n[3],
                m[2] * n[0] + m[3] * n[2], m[2] * n[1] + m[3] * n[3]};
    }

    static int transformOf(int manipulation) {
        int rotation = manipulation & ~(FLIP_HORIZONTAL | FLIP_VERTICAL);
        int[] r;
        if (rotation == 0) {
            r = TRANSFORMS[0];
        } else if (rotation == ROTATE_90) {
            r = TRANSFORMS[6];    /* counter-clockwise: MIDP's TRANS_ROT270 */
        } else if (rotation == ROTATE_180) {
            r = TRANSFORMS[3];
        } else if (rotation == ROTATE_270) {
            r = TRANSFORMS[5];
        } else {
            throw new IllegalArgumentException("manipulation " + manipulation);
        }
        int[] flip = {(manipulation & FLIP_HORIZONTAL) != 0 ? -1 : 1, 0, 0, (manipulation & FLIP_VERTICAL) != 0 ? -1 : 1};
        int[] m = multiply(r, flip);
        for (int t = 0; t < TRANSFORMS.length; t++) {
            int[] c = TRANSFORMS[t];
            if (c[0] == m[0] && c[1] == m[1] && c[2] == m[2] && c[3] == m[3]) {
                return t;
            }
        }
        throw new IllegalArgumentException("manipulation " + manipulation);
    }

    public void drawImage(Image img, int x, int y, int anchor, int manipulation) {
        if (img == null) {
            throw new NullPointerException();
        }
        g.drawRegion(img, 0, 0, img.getWidth(), img.getHeight(), transformOf(manipulation), x, y,
                anchor == 0 ? Graphics.TOP | Graphics.LEFT : anchor);
    }

    /** w x h ARGB pixels, transformed as drawRegion would; out[0], out[1] get the new size. */
    private static int[] transform(int[] argb, int w, int h, int transform, int[] size) {
        boolean turned = transform >= 4;
        int tw = turned ? h : w, th = turned ? w : h;
        int[] out = new int[tw * th];
        for (int dy = 0; dy < th; dy++) {
            for (int dx = 0; dx < tw; dx++) {
                int sx, sy;
                switch (transform) {
                case 1: sx = dx; sy = h - 1 - dy; break;
                case 2: sx = w - 1 - dx; sy = dy; break;
                case 3: sx = w - 1 - dx; sy = h - 1 - dy; break;
                case 4: sx = dy; sy = dx; break;
                case 5: sx = dy; sy = h - 1 - dx; break;
                case 6: sx = w - 1 - dy; sy = dx; break;
                case 7: sx = w - 1 - dy; sy = h - 1 - dx; break;
                default: sx = dx; sy = dy; break;
                }
                out[dy * tw + dx] = argb[sy * w + sx];
            }
        }
        size[0] = tw;
        size[1] = th;
        return out;
    }

    private void draw(int[] argb, int w, int h, int x, int y, int manipulation, boolean transparency) {
        int[] size = new int[2];
        int[] t = transform(argb, w, h, transformOf(manipulation), size);
        g.drawRGB(t, 0, size[0], x, y, size[0], size[1], transparency);
    }

    // ---- pixel formats

    private static int scale(int v, int bits) {
        return v * 255 / ((1 << bits) - 1);
    }

    private static int fromShort(int v, int format) {
        switch (format) {
        case TYPE_USHORT_4444_ARGB:
            return (scale((v >> 12) & 15, 4) << 24) | (scale((v >> 8) & 15, 4) << 16) | (scale((v >> 4) & 15, 4) << 8)
                    | scale(v & 15, 4);
        case TYPE_USHORT_444_RGB:
            return 0xFF000000 | (scale((v >> 8) & 15, 4) << 16) | (scale((v >> 4) & 15, 4) << 8) | scale(v & 15, 4);
        case TYPE_USHORT_555_RGB:
            return 0xFF000000 | (scale((v >> 10) & 31, 5) << 16) | (scale((v >> 5) & 31, 5) << 8) | scale(v & 31, 5);
        case TYPE_USHORT_1555_ARGB:
            return ((v & 0x8000) != 0 ? 0xFF000000 : 0) | (scale((v >> 10) & 31, 5) << 16)
                    | (scale((v >> 5) & 31, 5) << 8) | scale(v & 31, 5);
        case TYPE_USHORT_565_RGB:
            return 0xFF000000 | (scale((v >> 11) & 31, 5) << 16) | (scale((v >> 5) & 63, 6) << 8) | scale(v & 31, 5);
        default:
            throw new IllegalArgumentException("format " + format);
        }
    }

    private static int toShort(int argb, int format) {
        int a = argb >>> 24, r = (argb >> 16) & 0xFF, gr = (argb >> 8) & 0xFF, b = argb & 0xFF;
        switch (format) {
        case TYPE_USHORT_4444_ARGB: return ((a >> 4) << 12) | ((r >> 4) << 8) | ((gr >> 4) << 4) | (b >> 4);
        case TYPE_USHORT_444_RGB: return ((r >> 4) << 8) | ((gr >> 4) << 4) | (b >> 4);
        case TYPE_USHORT_555_RGB: return ((r >> 3) << 10) | ((gr >> 3) << 5) | (b >> 3);
        case TYPE_USHORT_1555_ARGB: return (a >= 128 ? 0x8000 : 0) | ((r >> 3) << 10) | ((gr >> 3) << 5) | (b >> 3);
        case TYPE_USHORT_565_RGB: return ((r >> 3) << 11) | ((gr >> 2) << 5) | (b >> 3);
        default: throw new IllegalArgumentException("format " + format);
        }
    }

    /** Bits per pixel of a byte format. */
    private static int bitsOf(int format) {
        switch (format) {
        case TYPE_BYTE_1_GRAY: case TYPE_BYTE_1_GRAY_VERTICAL: return 1;
        case TYPE_BYTE_2_GRAY: return 2;
        case TYPE_BYTE_4_GRAY: return 4;
        case TYPE_BYTE_8_GRAY: case TYPE_BYTE_332_RGB: return 8;
        default: throw new IllegalArgumentException("format " + format);
        }
    }

    /** Pixel (col, row) of a byte format: its value. Rows are scanlength
     *  pixels apart; the vertical format packs 8 rows a byte, top in bit 0. */
    private static int byteValue(byte[] data, int offset, int scanlength, int col, int row, int format) {
        if (format == TYPE_BYTE_1_GRAY_VERTICAL) {
            int b = data[offset + (row / 8) * scanlength + col] & 0xFF;
            return (b >> (row % 8)) & 1;
        }
        int bits = bitsOf(format);
        int p = offset + row * scanlength + col;   /* in pixels */
        int b = data[p * bits / 8] & 0xFF;
        int shift = 8 - bits - (p * bits) % 8;
        return (b >> shift) & ((1 << bits) - 1);
    }

    private static int fromByte(int v, int format) {
        if (format == TYPE_BYTE_1_GRAY || format == TYPE_BYTE_1_GRAY_VERTICAL) {
            return v != 0 ? 0xFF000000 : 0xFFFFFFFF;   /* 1 is black */
        }
        if (format == TYPE_BYTE_332_RGB) {
            return 0xFF000000 | (scale((v >> 5) & 7, 3) << 16) | (scale((v >> 2) & 7, 3) << 8) | scale(v & 3, 2);
        }
        int grey = scale(v, bitsOf(format));
        return 0xFF000000 | (grey << 16) | (grey << 8) | grey;
    }

    private static void checkRegion(int length, int offset, int scanlength, int width, int height) {
        if (width < 0 || height < 0 || offset < 0) {
            throw new IllegalArgumentException("region " + width + "x" + height);
        }
        if (width > 0 && height > 0 && (scanlength < width || offset + (long) (height - 1) * scanlength + width > length)) {
            throw new ArrayIndexOutOfBoundsException();
        }
    }

    // ---- drawing pixels

    public void drawPixels(int[] pixels, boolean transparency, int offset, int scanlength, int x, int y, int width,
            int height, int manipulation, int format) {
        if (format != TYPE_INT_8888_ARGB && format != TYPE_INT_888_RGB) {
            throw new IllegalArgumentException("format " + format);
        }
        checkRegion(pixels.length, offset, scanlength, width, height);
        int[] argb = new int[width * height];
        for (int r = 0; r < height; r++) {
            for (int c = 0; c < width; c++) {
                int v = pixels[offset + r * scanlength + c];
                argb[r * width + c] = format == TYPE_INT_888_RGB || !transparency ? v | 0xFF000000 : v;
            }
        }
        draw(argb, width, height, x, y, manipulation, transparency && format == TYPE_INT_8888_ARGB);
    }

    public void drawPixels(short[] pixels, boolean transparency, int offset, int scanlength, int x, int y,
            int width, int height, int manipulation, int format) {
        checkRegion(pixels.length, offset, scanlength, width, height);
        int[] argb = new int[width * height];
        for (int r = 0; r < height; r++) {
            for (int c = 0; c < width; c++) {
                int v = fromShort(pixels[offset + r * scanlength + c] & 0xFFFF, format);
                argb[r * width + c] = transparency ? v : v | 0xFF000000;
            }
        }
        draw(argb, width, height, x, y, manipulation, transparency);
    }

    public void drawPixels(byte[] pixels, byte[] transparencyMask, int offset, int scanlength, int x, int y,
            int width, int height, int manipulation, int format) {
        bitsOf(format);
        int[] argb = new int[width * height];
        for (int r = 0; r < height; r++) {
            for (int c = 0; c < width; c++) {
                int v = fromByte(byteValue(pixels, offset, scanlength, c, r, format), format);
                if (transparencyMask != null && byteValue(transparencyMask, offset, scanlength, c, r, format) == 0) {
                    v &= 0x00FFFFFF;
                }
                argb[r * width + c] = v;
            }
        }
        draw(argb, width, height, x, y, manipulation, transparencyMask != null);
    }

    // ---- reading pixels

    /** w x h pixels of the target at (x, y) (translated), as ARGB. */
    private int[] read(int x, int y, int width, int height) {
        Image target = LcduiAccess.get().target(g);
        int[] argb = new int[width * height];
        target.getRGB(argb, 0, width, x + g.getTranslateX(), y + g.getTranslateY(), width, height);
        return argb;
    }

    public void getPixels(int[] pixels, int offset, int scanlength, int x, int y, int width, int height, int format) {
        if (format != TYPE_INT_8888_ARGB && format != TYPE_INT_888_RGB) {
            throw new IllegalArgumentException("format " + format);
        }
        checkRegion(pixels.length, offset, scanlength, width, height);
        int[] argb = read(x, y, width, height);
        for (int r = 0; r < height; r++) {
            for (int c = 0; c < width; c++) {
                int v = argb[r * width + c];
                pixels[offset + r * scanlength + c] = format == TYPE_INT_888_RGB ? v & 0xFFFFFF : v;
            }
        }
    }

    public void getPixels(short[] pixels, int offset, int scanlength, int x, int y, int width, int height,
            int format) {
        checkRegion(pixels.length, offset, scanlength, width, height);
        int[] argb = read(x, y, width, height);
        for (int r = 0; r < height; r++) {
            for (int c = 0; c < width; c++) {
                pixels[offset + r * scanlength + c] = (short) toShort(argb[r * width + c], format);
            }
        }
    }

    public void getPixels(byte[] pixels, byte[] transparencyMask, int offset, int scanlength, int x, int y,
            int width, int height, int format) {
        int bits = bitsOf(format);
        int[] argb = read(x, y, width, height);
        for (int r = 0; r < height; r++) {
            for (int c = 0; c < width; c++) {
                int v = argb[r * width + c];
                int grey = (((v >> 16) & 0xFF) + ((v >> 8) & 0xFF) + (v & 0xFF)) / 3;
                int value;
                if (format == TYPE_BYTE_1_GRAY || format == TYPE_BYTE_1_GRAY_VERTICAL) {
                    value = grey < 128 ? 1 : 0;
                } else if (format == TYPE_BYTE_332_RGB) {
                    value = (((v >> 21) & 7) << 5) | (((v >> 13) & 7) << 2) | ((v >> 6) & 3);
                } else {
                    value = grey >> (8 - bits);
                }
                put(pixels, offset, scanlength, c, r, format, bits, value);
                if (transparencyMask != null) {
                    put(transparencyMask, offset, scanlength, c, r, format, bits, (v >>> 24) != 0 ? (1 << bits) - 1 : 0);
                }
            }
        }
    }

    private static void put(byte[] data, int offset, int scanlength, int col, int row, int format, int bits, int v) {
        if (format == TYPE_BYTE_1_GRAY_VERTICAL) {
            int i = offset + (row / 8) * scanlength + col;
            data[i] = (byte) ((data[i] & ~(1 << (row % 8))) | (v << (row % 8)));
            return;
        }
        int p = offset + row * scanlength + col;
        int i = p * bits / 8, shift = 8 - bits - (p * bits) % 8, mask = ((1 << bits) - 1) << shift;
        data[i] = (byte) ((data[i] & ~mask) | ((v << shift) & mask));
    }

    // ---- shapes in ARGB colours

    /** Draws a coverage mask (bounding box at x0, y0) in an ARGB colour. */
    private void fillMask(boolean[] mask, int x0, int y0, int w, int h, int argbColor) {
        int[] argb = new int[w * h];
        for (int i = 0; i < argb.length; i++) {
            argb[i] = mask[i] ? argbColor : 0;
        }
        g.drawRGB(argb, 0, w, x0, y0, w, h, true);
    }

    private static void line(boolean[] mask, int w, int x1, int y1, int x2, int y2) {
        int dx = Math.abs(x2 - x1), dy = -Math.abs(y2 - y1);
        int sx = x1 < x2 ? 1 : -1, sy = y1 < y2 ? 1 : -1, err = dx + dy;
        for (;;) {
            mask[y1 * w + x1] = true;
            if (x1 == x2 && y1 == y2) {
                return;
            }
            int e2 = 2 * err;
            if (e2 >= dy) {
                err += dy;
                x1 += sx;
            }
            if (e2 <= dx) {
                err += dx;
                y1 += sy;
            }
        }
    }

    /** Draws a polygon given relative to its bounding box, filled or not. */
    private void polygon(int[] xs, int[] ys, int n, int argbColor, boolean fill) {
        if (n < 2) {
            return;
        }
        int minX = xs[0], maxX = xs[0], minY = ys[0], maxY = ys[0];
        for (int i = 1; i < n; i++) {
            minX = Math.min(minX, xs[i]);
            maxX = Math.max(maxX, xs[i]);
            minY = Math.min(minY, ys[i]);
            maxY = Math.max(maxY, ys[i]);
        }
        int w = maxX - minX + 1, h = maxY - minY + 1;
        boolean[] mask = new boolean[w * h];
        if (fill) {   /* even-odd, between edge crossings on each row */
            int[] cross = new int[n];
            for (int y = minY; y <= maxY; y++) {
                int k = 0;
                for (int i = 0; i < n; i++) {
                    int xa = xs[i], ya = ys[i], xb = xs[(i + 1) % n], yb = ys[(i + 1) % n];
                    if (ya == yb || y < Math.min(ya, yb) || y >= Math.max(ya, yb)) {
                        continue;
                    }
                    cross[k++] = xa + (int) (((long) (y - ya) * (xb - xa) * 2 + (yb - ya)) / (2L * (yb - ya)));
                }
                for (int i = 1; i < k; i++) {   /* sort the crossings */
                    int v = cross[i], j = i - 1;
                    while (j >= 0 && cross[j] > v) {
                        cross[j + 1] = cross[j];
                        j--;
                    }
                    cross[j + 1] = v;
                }
                for (int i = 0; i + 1 < k; i += 2) {
                    for (int x = cross[i]; x <= cross[i + 1]; x++) {
                        mask[(y - minY) * w + (x - minX)] = true;
                    }
                }
            }
        }
        for (int i = 0; i < n; i++) {
            int j = (i + 1) % n;
            if (!fill && j == 0 && n == 2) {
                break;
            }
            line(mask, w, xs[i] - minX, ys[i] - minY, xs[j] - minX, ys[j] - minY);
        }
        fillMask(mask, minX, minY, w, h, argbColor);
    }

    public void drawTriangle(int x1, int y1, int x2, int y2, int x3, int y3, int argbColor) {
        polygon(new int[] {x1, x2, x3}, new int[] {y1, y2, y3}, 3, argbColor, false);
    }

    public void fillTriangle(int x1, int y1, int x2, int y2, int x3, int y3, int argbColor) {
        polygon(new int[] {x1, x2, x3}, new int[] {y1, y2, y3}, 3, argbColor, true);
    }

    private static int[] slice(int[] points, int offset, int n) {
        if (offset < 0 || n < 0 || offset > points.length - n) {
            throw new ArrayIndexOutOfBoundsException();
        }
        int[] out = new int[n];
        System.arraycopy(points, offset, out, 0, n);
        return out;
    }

    public void drawPolygon(int[] xPoints, int xOffset, int[] yPoints, int yOffset, int nPoints, int argbColor) {
        polygon(slice(xPoints, xOffset, nPoints), slice(yPoints, yOffset, nPoints), nPoints, argbColor, false);
    }

    public void fillPolygon(int[] xPoints, int xOffset, int[] yPoints, int yOffset, int nPoints, int argbColor) {
        polygon(slice(xPoints, xOffset, nPoints), slice(yPoints, yOffset, nPoints), nPoints, argbColor, true);
    }
}
