package javax.microedition.lcdui;

/** A screen of editable text; typed on the number keys (see TextModel). */
public class TextBox extends Screen {
    private final TextModel model;

    public TextBox(String title, String text, int maxSize, int constraints) {
        this.title = title;
        model = new TextModel(text, maxSize, constraints);
    }

    public String getString() {
        return model.get();
    }

    public void setString(String text) {
        model.set(text);
        Display.repaintIfShown(this);
    }

    public int getChars(char[] data) {
        String s = model.get();
        if (data.length < s.length()) {
            throw new ArrayIndexOutOfBoundsException();
        }
        s.getChars(0, s.length(), data, 0);
        return s.length();
    }

    public void setChars(char[] data, int offset, int length) {
        if (data == null) {
            setString("");
            return;
        }
        if (offset < 0 || length < 0 || offset > data.length - length) {
            throw new ArrayIndexOutOfBoundsException();
        }
        setString(new String(data, offset, length));
    }

    public void insert(String src, int position) {
        model.insert(src, position);
        Display.repaintIfShown(this);
    }

    public void insert(char[] data, int offset, int length, int position) {
        if (offset < 0 || length < 0 || offset > data.length - length) {
            throw new ArrayIndexOutOfBoundsException();
        }
        insert(new String(data, offset, length), position);
    }

    public void delete(int offset, int length) {
        model.delete(offset, length);
        Display.repaintIfShown(this);
    }

    public int getMaxSize() {
        return model.max;
    }

    public int setMaxSize(int maxSize) {
        int m = model.setMax(maxSize);
        Display.repaintIfShown(this);
        return m;
    }

    public int size() {
        return model.size();
    }

    public int getCaretPosition() {
        return model.caret;
    }

    public void setConstraints(int constraints) {
        model.setConstraints(constraints);
        Display.repaintIfShown(this);
    }

    public int getConstraints() {
        return model.constraints;
    }

    public void setInitialInputMode(String characterSubset) {
    }

    // ---- on the screen

    private String[] lines(int width) {
        return TextLines.wrap(model.shown(), Skin.TEXT, width - 2 * Skin.GAP - 6);
    }

    int contentHeight(int width) {
        return lines(width).length * Skin.TEXT.getHeight() + 2 * Skin.GAP + 4;
    }

    void paintContent(Graphics g, int width, int height) {
        int h = Math.max(height - 2 * Skin.GAP, contentHeight(width) - 2 * Skin.GAP);
        g.setColor(Skin.FIELD);
        g.fillRect(Skin.GAP, Skin.GAP, width - 2 * Skin.GAP, h);
        g.setColor(Skin.BORDER);
        g.drawRect(Skin.GAP, Skin.GAP, width - 2 * Skin.GAP - 1, h - 1);
        g.setColor(Skin.FOREGROUND);
        g.setFont(Skin.TEXT);
        String[] lines = lines(width);
        int fh = Skin.TEXT.getHeight(), at = 0;
        for (int i = 0; i < lines.length; i++) {
            int y = Skin.GAP + 2 + i * fh;
            g.drawString(lines[i], Skin.GAP + 3, y, Graphics.TOP | Graphics.LEFT);
            int caret = model.caret - at;
            if (caret >= 0 && caret <= lines[i].length() && (i == lines.length - 1 || caret < lines[i].length())) {
                int cx = Skin.GAP + 3 + Skin.TEXT.substringWidth(lines[i], 0, caret);
                g.setColor(Skin.HIGHLIGHT);
                g.drawLine(cx, y + 1, cx, y + fh - 1);
                g.setColor(Skin.FOREGROUND);
                at = Integer.MIN_VALUE / 2;
            }
            at += lines[i].length() + 1;
        }
    }

    void keyEvent(int type, int keyCode) {
        if (type == KEY_UP) {
            return;
        }
        if (model.key(keyCode, System.currentTimeMillis()) != 0) {
            Display.repaintIfShown(this);
            return;
        }
        super.keyEvent(type, keyCode);
    }
}
