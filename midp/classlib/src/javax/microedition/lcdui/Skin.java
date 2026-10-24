package javax.microedition.lcdui;

/**
 * The look of the high-level UI: colours, fonts and the bars around a
 * screen (title and ticker at the top, soft keys at the bottom).
 */
final class Skin {
    static final int BACKGROUND = 0xF4F4F4;
    static final int FOREGROUND = 0x1E1E1E;
    static final int MUTED = 0x6E6E6E;
    static final int HIGHLIGHT = 0x2F6FD0;
    static final int HIGHLIGHT_TEXT = 0xFFFFFF;
    static final int BORDER = 0x9A9A9A;
    static final int FIELD = 0xFFFFFF;
    static final int BAR = 0x1F2D4A;
    static final int BAR_TEXT = 0xFFFFFF;

    static final Font TEXT = Font.getDefaultFont();
    static final Font BOLD = Font.getFont(Font.FACE_SYSTEM, Font.STYLE_BOLD, Font.SIZE_MEDIUM);
    static final Font SMALL = Font.getFont(Font.FACE_SYSTEM, Font.STYLE_PLAIN, Font.SIZE_SMALL);

    static final int GAP = 3;   /* between and around items */

    private Skin() {
    }

    static int titleHeight() {
        return BOLD.getHeight() + 4;
    }

    static int tickerHeight() {
        return TEXT.getHeight() + 2;
    }

    static int barHeight() {
        return TEXT.getHeight() + 4;
    }

    static void title(Graphics g, String title, int y, int w) {
        g.setColor(BAR);
        g.fillRect(0, y, w, titleHeight());
        g.setColor(BAR_TEXT);
        g.setFont(BOLD);
        g.drawString(fit(title == null ? "" : title, BOLD, w - 8), w / 2, y + 2, Graphics.TOP | Graphics.HCENTER);
    }

    /** A ticker line; offset is how far the text has scrolled. */
    static void ticker(Graphics g, String text, int offset, int y, int w) {
        g.setColor(0xFFF2B0);
        g.fillRect(0, y, w, tickerHeight());
        g.setColor(FOREGROUND);
        g.setFont(TEXT);
        int clipX = g.getClipX(), clipY = g.getClipY(), clipW = g.getClipWidth(), clipH = g.getClipHeight();
        g.clipRect(0, y, w, tickerHeight());
        g.drawString(text, w - offset, y + 1, Graphics.TOP | Graphics.LEFT);
        g.setClip(clipX, clipY, clipW, clipH);
    }

    static void softBar(Graphics g, String left, String right, int w, int h) {
        int bh = barHeight(), y = h - bh;
        g.setColor(BAR);
        g.fillRect(0, y, w, bh);
        g.setColor(BAR_TEXT);
        g.setFont(TEXT);
        if (left != null) {
            g.drawString(fit(left, TEXT, w / 2 - 6), 4, y + 2, Graphics.TOP | Graphics.LEFT);
        }
        if (right != null) {
            g.drawString(fit(right, TEXT, w / 2 - 6), w - 4, y + 2, Graphics.TOP | Graphics.RIGHT);
        }
    }

    /** Text cut with "..." to fit a width. */
    static String fit(String s, Font f, int w) {
        if (f.stringWidth(s) <= w) {
            return s;
        }
        int dots = f.stringWidth("...");
        int n = s.length();
        while (n > 0 && f.substringWidth(s, 0, n) + dots > w) {
            n--;
        }
        return s.substring(0, n) + "...";
    }

    /** A focus frame around a rectangle. */
    static void focus(Graphics g, int x, int y, int w, int h) {
        g.setColor(HIGHLIGHT);
        g.drawRect(x, y, w - 1, h - 1);
        g.drawRect(x + 1, y + 1, w - 3, h - 3);
    }
}
