package javax.microedition.lcdui;

/**
 * The built-in fonts: three sizes in plain, bold, italic and bold italic,
 * optionally underlined. All faces look the same (there is one typeface).
 * Fonts are shared: the same request gives the same object.
 */
public final class Font {
    public static final int STYLE_PLAIN = 0;
    public static final int STYLE_BOLD = 1;
    public static final int STYLE_ITALIC = 2;
    public static final int STYLE_UNDERLINED = 4;
    public static final int SIZE_SMALL = 8;
    public static final int SIZE_MEDIUM = 0;
    public static final int SIZE_LARGE = 16;
    public static final int FACE_SYSTEM = 0;
    public static final int FACE_MONOSPACE = 32;
    public static final int FACE_PROPORTIONAL = 64;
    public static final int FONT_STATIC_TEXT = 0;
    public static final int FONT_INPUT_TEXT = 1;

    private static final Font[] CACHE = new Font[3 * 3 * 8];
    private static final Font DEFAULT = getFont(FACE_SYSTEM, STYLE_PLAIN, SIZE_MEDIUM);

    private final int face;
    private final int size;
    /* Read by the drawing natives: 0 small, 1 medium, 2 large; the style bits. */
    final int sizeIndex;
    final int style;
    private final int height;
    private final int baseline;

    private Font(int face, int style, int size) {
        this.face = face;
        this.style = style;
        this.size = size;
        this.sizeIndex = size == SIZE_SMALL ? 0 : size == SIZE_LARGE ? 2 : 1;
        this.height = height0(sizeIndex);
        this.baseline = baseline0(sizeIndex);
    }

    public static Font getDefaultFont() {
        return DEFAULT;
    }

    public static Font getFont(int fontSpecifier) {
        if (fontSpecifier != FONT_STATIC_TEXT && fontSpecifier != FONT_INPUT_TEXT) {
            throw new IllegalArgumentException("font specifier " + fontSpecifier);
        }
        return DEFAULT;
    }

    public static Font getFont(int face, int style, int size) {
        if (face != FACE_SYSTEM && face != FACE_MONOSPACE && face != FACE_PROPORTIONAL) {
            throw new IllegalArgumentException("face " + face);
        }
        if ((style & ~(STYLE_BOLD | STYLE_ITALIC | STYLE_UNDERLINED)) != 0) {
            throw new IllegalArgumentException("style " + style);
        }
        if (size != SIZE_SMALL && size != SIZE_MEDIUM && size != SIZE_LARGE) {
            throw new IllegalArgumentException("size " + size);
        }
        int i = ((face >> 5) * 3 + (size == SIZE_SMALL ? 0 : size == SIZE_LARGE ? 2 : 1)) * 8 + style;
        synchronized (CACHE) {
            if (CACHE[i] == null) {
                CACHE[i] = new Font(face, style, size);
            }
            return CACHE[i];
        }
    }

    public int getStyle() {
        return style;
    }

    public int getSize() {
        return size;
    }

    public int getFace() {
        return face;
    }

    public boolean isPlain() {
        return style == STYLE_PLAIN;
    }

    public boolean isBold() {
        return (style & STYLE_BOLD) != 0;
    }

    public boolean isItalic() {
        return (style & STYLE_ITALIC) != 0;
    }

    public boolean isUnderlined() {
        return (style & STYLE_UNDERLINED) != 0;
    }

    public int getHeight() {
        return height;
    }

    public int getBaselinePosition() {
        return baseline;
    }

    public int charWidth(char ch) {
        return charWidth0(sizeIndex, style, ch);
    }

    public int charsWidth(char[] ch, int offset, int length) {
        if (offset < 0 || length < 0 || offset > ch.length - length) {
            throw new ArrayIndexOutOfBoundsException();
        }
        return charsWidth0(sizeIndex, style, ch, offset, length);
    }

    public int stringWidth(String str) {
        return stringWidth0(sizeIndex, style, str, 0, str.length());
    }

    public int substringWidth(String str, int offset, int len) {
        if (offset < 0 || len < 0 || offset > str.length() - len) {
            throw new StringIndexOutOfBoundsException();
        }
        return stringWidth0(sizeIndex, style, str, offset, len);
    }

    private static native int charWidth0(int size, int style, char ch);

    private static native int charsWidth0(int size, int style, char[] ch, int off, int len);

    private static native int stringWidth0(int size, int style, String s, int off, int len);

    private static native int height0(int size);

    private static native int baseline0(int size);
}
