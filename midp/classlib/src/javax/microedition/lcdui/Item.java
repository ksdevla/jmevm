package javax.microedition.lcdui;

import java.util.Vector;

/**
 * Something on a form (or an alert's indicator). Each item takes a row of
 * the form: its label, then its content.
 */
public abstract class Item {
    public static final int LAYOUT_DEFAULT = 0;
    public static final int LAYOUT_LEFT = 1;
    public static final int LAYOUT_RIGHT = 2;
    public static final int LAYOUT_CENTER = 3;
    public static final int LAYOUT_TOP = 0x10;
    public static final int LAYOUT_BOTTOM = 0x20;
    public static final int LAYOUT_VCENTER = 0x30;
    public static final int LAYOUT_NEWLINE_BEFORE = 0x100;
    public static final int LAYOUT_NEWLINE_AFTER = 0x200;
    public static final int LAYOUT_SHRINK = 0x400;
    public static final int LAYOUT_EXPAND = 0x800;
    public static final int LAYOUT_VSHRINK = 0x1000;
    public static final int LAYOUT_VEXPAND = 0x2000;
    public static final int LAYOUT_2 = 0x4000;
    public static final int PLAIN = 0;
    public static final int HYPERLINK = 1;
    public static final int BUTTON = 2;

    private static final int LAYOUT_BITS = 0x7F33;

    String label;
    private int layout;
    Screen owner;
    final Vector commands = new Vector();
    Command defaultCommand;
    ItemCommandListener itemListener;
    private int preferredWidth = -1, preferredHeight = -1;

    Item(String label) {
        this.label = label;
    }

    public void setLabel(String label) {
        this.label = label;
        changed();
    }

    public String getLabel() {
        return label;
    }

    public int getLayout() {
        return layout;
    }

    public void setLayout(int layout) {
        if ((layout & ~LAYOUT_BITS) != 0) {
            throw new IllegalArgumentException("layout " + layout);
        }
        this.layout = layout;
        changed();
    }

    public void addCommand(Command cmd) {
        if (cmd == null) {
            throw new NullPointerException();
        }
        if (!commands.contains(cmd)) {
            commands.addElement(cmd);
        }
        changed();
    }

    public void removeCommand(Command cmd) {
        commands.removeElement(cmd);
        if (cmd == defaultCommand) {
            defaultCommand = null;
        }
        changed();
    }

    public void setItemCommandListener(ItemCommandListener l) {
        itemListener = l;
    }

    public void setDefaultCommand(Command cmd) {
        defaultCommand = cmd;
        if (cmd != null && !commands.contains(cmd)) {
            commands.addElement(cmd);
        }
        changed();
    }

    public int getPreferredWidth() {
        return preferredWidth >= 0 ? preferredWidth : Display.width - 2 * Skin.GAP;
    }

    public int getPreferredHeight() {
        return preferredHeight >= 0 ? preferredHeight : height(getPreferredWidth());
    }

    public int getMinimumWidth() {
        return Skin.TEXT.charWidth('W') * 2;
    }

    public int getMinimumHeight() {
        return Skin.TEXT.getHeight();
    }

    public void setPreferredSize(int width, int height) {
        if (width < -1 || height < -1) {
            throw new IllegalArgumentException("preferred size " + width + "x" + height);
        }
        preferredWidth = width;
        preferredHeight = height;
        changed();
    }

    public void notifyStateChanged() {
        if (!(owner instanceof Form)) {
            throw new IllegalStateException("the item is on no form");
        }
        ((Form) owner).stateChanged(this);
    }

    // ---- for Form

    /** The height of the item's row: label and content. */
    int height(int width) {
        return labelHeight(width) + contentHeight(width);
    }

    int labelHeight(int width) {
        return label == null || label.length() == 0 ? 0 : TextLines.wrap(label, Skin.BOLD, width).length * Skin.BOLD.getHeight();
    }

    abstract int contentHeight(int width);

    /** Paints the content at (0, 0), focused or not. */
    abstract void paintContent(Graphics g, int width, int height, boolean focused);

    void paint(Graphics g, int width, boolean focused) {
        int lh = labelHeight(width);
        if (lh > 0) {
            g.setColor(Skin.FOREGROUND);
            g.setFont(Skin.BOLD);
            String[] lines = TextLines.wrap(label, Skin.BOLD, width);
            for (int i = 0; i < lines.length; i++) {
                g.drawString(lines[i], 0, i * Skin.BOLD.getHeight(), Graphics.TOP | Graphics.LEFT);
            }
        }
        g.translate(0, lh);
        paintContent(g, width, contentHeight(width), focused);
        g.translate(0, -lh);
    }

    /** Whether the item can take the focus. */
    boolean focusable() {
        return commands.size() > 0;
    }

    /** A key while focused; true if the item used it (else the form moves
     *  the focus or scrolls). */
    boolean key(int type, int keyCode) {
        if (type == Displayable.KEY_DOWN && Canvas.actionOf(keyCode) == Canvas.FIRE && defaultCommand != null) {
            fireCommand(defaultCommand);
            return true;
        }
        return false;
    }

    /** A touch on the content (coordinates within it) while focused. */
    void pointer(int type, int x, int y, int width, int height) {
        if (type == jmevm.midp.Events.POINTER_UP && defaultCommand != null) {
            fireCommand(defaultCommand);
        }
    }

    void fireCommand(Command c) {
        ItemCommandListener l = itemListener;
        if (l != null) {
            l.commandAction(c, this);
        }
    }

    void focusChanged(boolean focused) {
    }

    /** Something visible changed: repaint the form. */
    void changed() {
        Screen s = owner;
        if (s != null) {
            Display.repaintIfShown(s);
        }
    }

    /** The user changed the item: changed, and the form's listener hears of it. */
    void userChanged() {
        changed();
        if (owner instanceof Form) {
            ((Form) owner).stateChanged(this);
        }
    }

    long nextTime() {
        return -1;
    }

    void timeStep(long now) {
    }
}
