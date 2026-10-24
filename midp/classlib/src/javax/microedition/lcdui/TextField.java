package javax.microedition.lcdui;

/** Editable text on a form; typed on the number keys (see TextModel). */
public class TextField extends Item {
    public static final int ANY = 0;
    public static final int EMAILADDR = 1;
    public static final int NUMERIC = 2;
    public static final int PHONENUMBER = 3;
    public static final int URL = 4;
    public static final int DECIMAL = 5;
    public static final int PASSWORD = 0x10000;
    public static final int UNEDITABLE = 0x20000;
    public static final int SENSITIVE = 0x40000;
    public static final int NON_PREDICTIVE = 0x80000;
    public static final int INITIAL_CAPS_WORD = 0x100000;
    public static final int INITIAL_CAPS_SENTENCE = 0x200000;
    public static final int CONSTRAINT_MASK = 0xFFFF;

    private final TextModel model;
    private boolean focused;

    public TextField(String label, String text, int maxSize, int constraints) {
        super(label);
        model = new TextModel(text, maxSize, constraints);
    }

    public String getString() {
        return model.get();
    }

    public void setString(String text) {
        model.set(text);
        changed();
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
        changed();
    }

    public void insert(char[] data, int offset, int length, int position) {
        if (offset < 0 || length < 0 || offset > data.length - length) {
            throw new ArrayIndexOutOfBoundsException();
        }
        insert(new String(data, offset, length), position);
    }

    public void delete(int offset, int length) {
        model.delete(offset, length);
        changed();
    }

    public int getMaxSize() {
        return model.max;
    }

    public int setMaxSize(int maxSize) {
        int m = model.setMax(maxSize);
        changed();
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
        changed();
    }

    public int getConstraints() {
        return model.constraints;
    }

    public void setInitialInputMode(String characterSubset) {
    }

    // ---- on a form

    boolean focusable() {
        return true;
    }

    void focusChanged(boolean f) {
        focused = f;
    }

    private String[] lines(int width) {
        return TextLines.wrap(model.shown(), Skin.TEXT, width - 6);
    }

    int contentHeight(int width) {
        return lines(width).length * Skin.TEXT.getHeight() + 4;
    }

    void paintContent(Graphics g, int width, int height, boolean focusedNow) {
        g.setColor(Skin.FIELD);
        g.fillRect(0, 0, width, height);
        g.setColor(Skin.BORDER);
        g.drawRect(0, 0, width - 1, height - 1);
        g.setColor((model.constraints & UNEDITABLE) != 0 ? Skin.MUTED : Skin.FOREGROUND);
        g.setFont(Skin.TEXT);
        String[] lines = lines(width);
        int fh = Skin.TEXT.getHeight(), seen = 0;
        for (int i = 0; i < lines.length; i++) {
            g.drawString(lines[i], 3, 2 + i * fh, Graphics.TOP | Graphics.LEFT);
            int caret = model.caret - seen;
            if (focusedNow && caret >= 0 && (caret <= lines[i].length() && (i == lines.length - 1 || caret < lines[i].length()))) {
                int cx = 3 + Skin.TEXT.substringWidth(lines[i], 0, Math.min(caret, lines[i].length()));
                g.setColor(Skin.HIGHLIGHT);
                g.drawLine(cx, 3 + i * fh, cx, 1 + (i + 1) * fh);
                g.setColor(Skin.FOREGROUND);
                seen = Integer.MIN_VALUE / 2;   /* drawn */
            }
            seen += lines[i].length() + 1;
        }
    }

    boolean key(int type, int keyCode) {
        if (type == Displayable.KEY_UP) {
            return keyCode >= Canvas.KEY_NUM0 && keyCode <= Canvas.KEY_NUM9;
        }
        int r = model.key(keyCode, System.currentTimeMillis());
        if (r == 2) {
            userChanged();
        } else if (r == 1) {
            changed();
        }
        return r != 0 || super.key(type, keyCode);
    }
}
