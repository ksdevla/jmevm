package javax.microedition.lcdui;

/** Text on a form; as a hyperlink or button it acts through its commands. */
public class StringItem extends Item {
    private String text;
    private final int appearance;
    private Font font = Skin.TEXT;

    public StringItem(String label, String text) {
        this(label, text, PLAIN);
    }

    public StringItem(String label, String text, int appearanceMode) {
        super(label);
        if (appearanceMode != PLAIN && appearanceMode != HYPERLINK && appearanceMode != BUTTON) {
            throw new IllegalArgumentException("appearance " + appearanceMode);
        }
        this.text = text;
        this.appearance = appearanceMode;
    }

    public String getText() {
        return text;
    }

    public void setText(String text) {
        this.text = text;
        changed();
    }

    public int getAppearanceMode() {
        return appearance;
    }

    public void setFont(Font font) {
        this.font = font != null ? font : Skin.TEXT;
        changed();
    }

    public Font getFont() {
        return font;
    }

    int contentHeight(int width) {
        if (text == null || text.length() == 0) {
            return 0;
        }
        int lines = TextLines.wrap(text, font, width - (appearance == BUTTON ? 8 : 0)).length;
        return lines * font.getHeight() + (appearance == BUTTON ? 6 : 0);
    }

    void paintContent(Graphics g, int width, int height, boolean focused) {
        if (text == null || text.length() == 0) {
            return;
        }
        int inset = 0;
        if (appearance == BUTTON) {
            g.setColor(focused ? Skin.HIGHLIGHT : 0xDADADA);
            g.fillRoundRect(0, 0, width, height, 8, 8);
            g.setColor(Skin.BORDER);
            g.drawRoundRect(0, 0, width - 1, height - 1, 8, 8);
            inset = 4;
        }
        g.setFont(font);
        g.setColor(appearance == BUTTON && focused ? Skin.HIGHLIGHT_TEXT
                : appearance == HYPERLINK ? Skin.HIGHLIGHT : Skin.FOREGROUND);
        String[] lines = TextLines.wrap(text, font, width - 2 * inset);
        int align = getLayout() & LAYOUT_CENTER;
        for (int i = 0; i < lines.length; i++) {
            int y = inset - (appearance == BUTTON ? 1 : 0) + i * font.getHeight();
            if (align == LAYOUT_CENTER || appearance == BUTTON) {
                g.drawString(lines[i], width / 2, y, Graphics.TOP | Graphics.HCENTER);
            } else if (align == LAYOUT_RIGHT) {
                g.drawString(lines[i], width, y, Graphics.TOP | Graphics.RIGHT);
            } else {
                g.drawString(lines[i], 0, y, Graphics.TOP | Graphics.LEFT);
            }
            if (appearance == HYPERLINK) {
                int lw = font.stringWidth(lines[i]);
                g.drawLine(0, y + font.getBaselinePosition() + 1, lw, y + font.getBaselinePosition() + 1);
            }
        }
    }
}
