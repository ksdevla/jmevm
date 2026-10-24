package javax.microedition.lcdui;

/**
 * Draws on an image (a mutable image, or the screen's back buffer for a
 * canvas). The drawing itself is native; this class keeps the state the
 * natives read: the target, translation, clip (absolute, x2 and y2
 * exclusive), colour, stroke and font, and resolves anchors.
 */
public class Graphics {
    public static final int HCENTER = 1;
    public static final int VCENTER = 2;
    public static final int LEFT = 4;
    public static final int RIGHT = 8;
    public static final int TOP = 16;
    public static final int BOTTOM = 32;
    public static final int BASELINE = 64;
    public static final int SOLID = 0;
    public static final int DOTTED = 1;

    /* Read by the natives in src/midp_jme/lcdui.c. */
    Image target;
    int tx, ty;
    int cx1, cy1, cx2, cy2;
    int pixel = 0xFF000000;
    int stroke = SOLID;
    Font font = Font.getDefaultFont();

    private final boolean screen;   /* the display: copyArea is not allowed */

    Graphics(Image target, boolean screen) {
        this.target = target;
        this.screen = screen;
        cx2 = target.width;
        cy2 = target.height;
    }

    /** Back to the state a canvas's paint() starts with, clipped to a region. */
    void reset(int x, int y, int w, int h) {
        tx = ty = 0;
        pixel = 0xFF000000;
        stroke = SOLID;
        font = Font.getDefaultFont();
        cx1 = Math.max(0, x);
        cy1 = Math.max(0, y);
        cx2 = Math.min(target.width, x + w);
        cy2 = Math.min(target.height, y + h);
    }

    // ---- translation and clip

    public void translate(int x, int y) {
        tx += x;
        ty += y;
    }

    public int getTranslateX() {
        return tx;
    }

    public int getTranslateY() {
        return ty;
    }

    public int getClipX() {
        return cx1 - tx;
    }

    public int getClipY() {
        return cy1 - ty;
    }

    public int getClipWidth() {
        return Math.max(0, cx2 - cx1);
    }

    public int getClipHeight() {
        return Math.max(0, cy2 - cy1);
    }

    public void clipRect(int x, int y, int width, int height) {
        int x1 = x + tx, y1 = y + ty;
        cx1 = Math.max(cx1, x1);
        cy1 = Math.max(cy1, y1);
        cx2 = Math.min(cx2, x1 + Math.max(0, width));
        cy2 = Math.min(cy2, y1 + Math.max(0, height));
    }

    public void setClip(int x, int y, int width, int height) {
        int x1 = x + tx, y1 = y + ty;
        cx1 = Math.max(0, x1);
        cy1 = Math.max(0, y1);
        cx2 = Math.min(target.width, x1 + Math.max(0, width));
        cy2 = Math.min(target.height, y1 + Math.max(0, height));
    }

    // ---- colour, stroke, font

    public void setColor(int red, int green, int blue) {
        if ((red | green | blue) < 0 || red > 255 || green > 255 || blue > 255) {
            throw new IllegalArgumentException("colour component out of range");
        }
        pixel = 0xFF000000 | (red << 16) | (green << 8) | blue;
    }

    public void setColor(int rgb) {
        pixel = 0xFF000000 | rgb;
    }

    public int getColor() {
        return pixel & 0xFFFFFF;
    }

    public int getRedComponent() {
        return (pixel >> 16) & 0xFF;
    }

    public int getGreenComponent() {
        return (pixel >> 8) & 0xFF;
    }

    public int getBlueComponent() {
        return pixel & 0xFF;
    }

    public void setGrayScale(int value) {
        if (value < 0 || value > 255) {
            throw new IllegalArgumentException("grey " + value);
        }
        setColor(value, value, value);
    }

    /** The grey nearest the colour (the average of its components). */
    public int getGrayScale() {
        return (getRedComponent() + getGreenComponent() + getBlueComponent()) / 3;
    }

    public int getDisplayColor(int color) {
        return color & 0xFFFFFF;
    }

    public void setStrokeStyle(int style) {
        if (style != SOLID && style != DOTTED) {
            throw new IllegalArgumentException("stroke " + style);
        }
        stroke = style;
    }

    public int getStrokeStyle() {
        return stroke;
    }

    public void setFont(Font f) {
        font = f != null ? f : Font.getDefaultFont();
    }

    public Font getFont() {
        return font;
    }

    // ---- shapes (native)

    public native void drawLine(int x1, int y1, int x2, int y2);

    public native void fillRect(int x, int y, int width, int height);

    public native void drawRect(int x, int y, int width, int height);

    public native void drawRoundRect(int x, int y, int width, int height, int arcWidth, int arcHeight);

    public native void fillRoundRect(int x, int y, int width, int height, int arcWidth, int arcHeight);

    public native void fillArc(int x, int y, int width, int height, int startAngle, int arcAngle);

    public native void drawArc(int x, int y, int width, int height, int startAngle, int arcAngle);

    public native void fillTriangle(int x1, int y1, int x2, int y2, int x3, int y3);

    // ---- text

    /** The left edge for a text anchor, after checking it. */
    private int textLeft(int x, int width, int anchor) {
        if (anchor == 0) {
            return x;
        }
        int h = anchor & (LEFT | RIGHT | HCENTER), v = anchor & (TOP | BOTTOM | BASELINE);
        if ((anchor & ~(LEFT | RIGHT | HCENTER | TOP | BOTTOM | BASELINE)) != 0 || !single(h) || !single(v)) {
            throw new IllegalArgumentException("text anchor " + anchor);
        }
        return h == RIGHT ? x - width : h == HCENTER ? x - width / 2 : x;
    }

    private int textTop(int y, int anchor) {
        int v = anchor & (TOP | BOTTOM | BASELINE);
        return v == BOTTOM ? y - font.getHeight() : v == BASELINE ? y - font.getBaselinePosition() : y;
    }

    /** At most one bit set. */
    private static boolean single(int bits) {
        return (bits & (bits - 1)) == 0;
    }

    public void drawString(String str, int x, int y, int anchor) {
        int w = font.stringWidth(str);   /* null: NullPointerException */
        drawString0(str, 0, str.length(), textLeft(x, w, anchor), textTop(y, anchor));
    }

    public void drawSubstring(String str, int offset, int len, int x, int y, int anchor) {
        int w = font.substringWidth(str, offset, len);
        drawString0(str, offset, len, textLeft(x, w, anchor), textTop(y, anchor));
    }

    public void drawChar(char character, int x, int y, int anchor) {
        drawChars(new char[] {character}, 0, 1, x, y, anchor);
    }

    public void drawChars(char[] data, int offset, int length, int x, int y, int anchor) {
        int w = font.charsWidth(data, offset, length);
        drawChars0(data, offset, length, textLeft(x, w, anchor), textTop(y, anchor));
    }

    private native void drawString0(String s, int off, int len, int x, int y);

    private native void drawChars0(char[] c, int off, int len, int x, int y);

    // ---- images

    /** The top left for an image anchor, after checking it. */
    private int imageLeft(int x, int width, int anchor) {
        if (anchor == 0) {
            return x;
        }
        int h = anchor & (LEFT | RIGHT | HCENTER), v = anchor & (TOP | BOTTOM | VCENTER);
        if ((anchor & ~(LEFT | RIGHT | HCENTER | TOP | BOTTOM | VCENTER)) != 0 || !single(h) || !single(v)) {
            throw new IllegalArgumentException("image anchor " + anchor);
        }
        return h == RIGHT ? x - width : h == HCENTER ? x - width / 2 : x;
    }

    private static int imageTop(int y, int height, int anchor) {
        int v = anchor & (TOP | BOTTOM | VCENTER);
        return v == BOTTOM ? y - height : v == VCENTER ? y - height / 2 : y;
    }

    public void drawImage(Image img, int x, int y, int anchor) {
        int left = imageLeft(x, img.width, anchor);   /* null: NullPointerException */
        drawRegion0(img, 0, 0, img.width, img.height, 0, left, imageTop(y, img.height, anchor));
    }

    public void drawRegion(Image src, int xSrc, int ySrc, int width, int height, int transform, int xDest,
            int yDest, int anchor) {
        if (src == null) {
            throw new NullPointerException();
        }
        if (src == target) {
            throw new IllegalArgumentException("source and destination are the same image");
        }
        if (transform < 0 || transform > 7) {
            throw new IllegalArgumentException("transform " + transform);
        }
        boolean turned = transform >= 4;   /* the transforms that swap width and height */
        int w = turned ? height : width, h = turned ? width : height;
        int left = imageLeft(xDest, w, anchor);
        drawRegion0(src, xSrc, ySrc, width, height, transform, left, imageTop(yDest, h, anchor));
    }

    public void copyArea(int xSrc, int ySrc, int width, int height, int xDest, int yDest, int anchor) {
        if (screen) {
            throw new IllegalStateException("copyArea on the display");
        }
        int left = imageLeft(xDest, width, anchor);
        copyArea0(xSrc, ySrc, width, height, left, imageTop(yDest, height, anchor));
    }

    public void drawRGB(int[] rgbData, int offset, int scanlength, int x, int y, int width, int height,
            boolean processAlpha) {
        if (rgbData == null) {
            throw new NullPointerException();
        }
        drawRGB0(rgbData, offset, scanlength, x, y, width, height, processAlpha);
    }

    native void drawRegion0(Image src, int sx, int sy, int w, int h, int transform, int x, int y);

    private native void copyArea0(int sx, int sy, int w, int h, int dx, int dy);

    private native void drawRGB0(int[] rgb, int offset, int scan, int x, int y, int w, int h, boolean alpha);
}
