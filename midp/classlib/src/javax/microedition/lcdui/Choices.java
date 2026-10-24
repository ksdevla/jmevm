package javax.microedition.lcdui;

import java.util.Vector;

/**
 * The elements of a List or ChoiceGroup, their selection, and how they are
 * drawn: a row each, with a radio button or check box where the type has
 * one. One element is highlighted (where the user is); in the exclusive
 * types exactly one is selected while there are any.
 */
final class Choices {
    private final int type;
    private final Vector strings = new Vector();
    private final Vector images = new Vector();
    private final Vector fonts = new Vector();
    private boolean[] selected = new boolean[4];
    private int fitPolicy = Choice.TEXT_WRAP_DEFAULT;
    int highlight;

    Choices(int type, String[] strings, Image[] images) {
        this.type = type;
        if (strings != null) {
            if (images != null && images.length != strings.length) {
                throw new IllegalArgumentException("as many images as strings are needed");
            }
            for (int i = 0; i < strings.length; i++) {
                append(strings[i], images == null ? null : images[i]);
            }
        }
    }

    private boolean exclusive() {
        return type != Choice.MULTIPLE;
    }

    private void check(int i) {
        if (i < 0 || i >= strings.size()) {
            throw new IndexOutOfBoundsException("element " + i);
        }
    }

    int size() {
        return strings.size();
    }

    String getString(int i) {
        check(i);
        return (String) strings.elementAt(i);
    }

    Image getImage(int i) {
        check(i);
        return (Image) images.elementAt(i);
    }

    int append(String s, Image img) {
        insert(strings.size(), s, img);
        return strings.size() - 1;
    }

    void insert(int i, String s, Image img) {
        if (s == null) {
            throw new NullPointerException();
        }
        if (i < 0 || i > strings.size()) {
            throw new IndexOutOfBoundsException("element " + i);
        }
        int n = strings.size();
        if (n == selected.length) {
            boolean[] more = new boolean[n * 2];
            System.arraycopy(selected, 0, more, 0, n);
            selected = more;
        }
        System.arraycopy(selected, i, selected, i + 1, n - i);
        selected[i] = false;
        strings.insertElementAt(s, i);
        images.insertElementAt(img, i);
        fonts.insertElementAt(null, i);
        if (exclusive() && n == 0) {
            selected[0] = true;
        }
        if (highlight >= i && n > 0) {
            highlight++;
        }
    }

    void delete(int i) {
        check(i);
        boolean wasSelected = selected[i];
        int n = strings.size();
        System.arraycopy(selected, i + 1, selected, i, n - i - 1);
        selected[n - 1] = false;
        strings.removeElementAt(i);
        images.removeElementAt(i);
        fonts.removeElementAt(i);
        n--;
        if (exclusive() && wasSelected && n > 0) {
            selected[Math.min(i, n - 1)] = true;
        }
        if (highlight > i || highlight >= n) {
            highlight = Math.max(0, highlight - 1);
        }
    }

    void deleteAll() {
        strings.removeAllElements();
        images.removeAllElements();
        fonts.removeAllElements();
        selected = new boolean[4];
        highlight = 0;
    }

    void set(int i, String s, Image img) {
        check(i);
        if (s == null) {
            throw new NullPointerException();
        }
        strings.setElementAt(s, i);
        images.setElementAt(img, i);
    }

    boolean isSelected(int i) {
        check(i);
        return selected[i];
    }

    int getSelectedIndex() {
        if (!exclusive()) {
            return -1;
        }
        for (int i = 0; i < strings.size(); i++) {
            if (selected[i]) {
                return i;
            }
        }
        return -1;
    }

    int getSelectedFlags(boolean[] flags) {
        if (flags.length < strings.size()) {
            throw new IllegalArgumentException("the array is shorter than the choice");
        }
        int count = 0;
        for (int i = 0; i < flags.length; i++) {
            flags[i] = i < strings.size() && selected[i];
            if (flags[i]) {
                count++;
            }
        }
        return count;
    }

    void setSelectedIndex(int i, boolean on) {
        check(i);
        if (exclusive()) {
            if (on) {   /* deselecting is ignored: one must stay selected */
                for (int k = 0; k < strings.size(); k++) {
                    selected[k] = k == i;
                }
                if (type == Choice.IMPLICIT) {
                    highlight = i;
                }
            }
        } else {
            selected[i] = on;
        }
    }

    void setSelectedFlags(boolean[] flags) {
        if (flags.length < strings.size()) {
            throw new IllegalArgumentException("the array is shorter than the choice");
        }
        if (!exclusive()) {
            System.arraycopy(flags, 0, selected, 0, strings.size());
            return;
        }
        int first = 0;
        for (int i = strings.size() - 1; i >= 0; i--) {
            if (flags[i]) {
                first = i;
            }
        }
        if (strings.size() > 0) {
            setSelectedIndex(first, true);
        }
    }

    void setFitPolicy(int policy) {
        if (policy < Choice.TEXT_WRAP_DEFAULT || policy > Choice.TEXT_WRAP_OFF) {
            throw new IllegalArgumentException("fit policy " + policy);
        }
        fitPolicy = policy;
    }

    int getFitPolicy() {
        return fitPolicy;
    }

    void setFont(int i, Font f) {
        check(i);
        fonts.setElementAt(f, i);
    }

    Font getFont(int i) {
        check(i);
        Font f = (Font) fonts.elementAt(i);
        return f != null ? f : Skin.TEXT;
    }

    // ---- drawing and input

    private int markWidth() {
        return type == Choice.EXCLUSIVE || type == Choice.MULTIPLE || type == Choice.POPUP ? 14 : 0;
    }

    private String[] lines(int i, int width) {
        Font f = getFont(i);
        Image img = getImage(i);
        int w = width - markWidth() - (img != null ? img.getWidth() + 3 : 0) - 4;
        String s = getString(i);
        return fitPolicy == Choice.TEXT_WRAP_ON ? TextLines.wrap(s, f, w) : new String[] {Skin.fit(s, f, w)};
    }

    int rowHeight(int i, int width) {
        Image img = getImage(i);
        int text = lines(i, width).length * getFont(i).getHeight();
        return Math.max(text, img != null ? img.getHeight() : 0) + 4;
    }

    int top(int index, int width) {
        int y = 0;
        for (int i = 0; i < index; i++) {
            y += rowHeight(i, width);
        }
        return y;
    }

    int height(int width) {
        return top(strings.size(), width);
    }

    void paint(Graphics g, int width, boolean active) {
        int y = 0;
        for (int i = 0; i < strings.size(); i++) {
            int h = rowHeight(i, width);
            boolean hl = active && i == highlight;
            g.setColor(hl ? Skin.HIGHLIGHT : Skin.FIELD);
            g.fillRect(0, y, width, h);
            int x = 2;
            int ink = hl ? Skin.HIGHLIGHT_TEXT : Skin.FOREGROUND;
            g.setColor(ink);
            if (type == Choice.EXCLUSIVE || type == Choice.POPUP) {
                g.drawArc(x, y + h / 2 - 5, 9, 9, 0, 360);
                if (selected[i]) {
                    g.fillArc(x + 3, y + h / 2 - 2, 4, 4, 0, 360);
                }
            } else if (type == Choice.MULTIPLE) {
                g.drawRect(x, y + h / 2 - 5, 9, 9);
                if (selected[i]) {
                    g.drawLine(x + 2, y + h / 2, x + 4, y + h / 2 + 2);
                    g.drawLine(x + 4, y + h / 2 + 2, x + 8, y + h / 2 - 3);
                }
            }
            x += markWidth();
            Image img = getImage(i);
            if (img != null) {
                g.drawImage(img, x, y + 2, Graphics.TOP | Graphics.LEFT);
                x += img.getWidth() + 3;
            }
            Font f = getFont(i);
            g.setFont(f);
            g.setColor(ink);
            String[] ls = lines(i, width);
            for (int k = 0; k < ls.length; k++) {
                g.drawString(ls[k], x, y + 2 + k * f.getHeight(), Graphics.TOP | Graphics.LEFT);
            }
            y += h;
        }
    }

    /** Moves the highlight; false at either end. */
    boolean move(int dir) {
        int to = highlight + dir;
        if (to < 0 || to >= strings.size()) {
            return false;
        }
        highlight = to;
        if (type == Choice.IMPLICIT) {
            setSelectedIndex(to, true);
        }
        return true;
    }

    /** Acts on the highlighted element: selects it, or toggles it in a
     *  multiple choice. False when there is none. */
    boolean act() {
        if (highlight >= strings.size()) {
            return false;
        }
        setSelectedIndex(highlight, type == Choice.MULTIPLE ? !selected[highlight] : true);
        return true;
    }

    /** The element at content row y, or -1. */
    int at(int y, int width) {
        int top = 0;
        for (int i = 0; i < strings.size(); i++) {
            int h = rowHeight(i, width);
            if (y >= top && y < top + h) {
                return i;
            }
            top += h;
        }
        return -1;
    }
}
