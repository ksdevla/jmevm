package javax.microedition.lcdui;

import java.util.Vector;

import jmevm.midp.Events;

/**
 * The menu a soft key opens when it has several commands: a list above the
 * soft-key bar. Up and down move, fire or the left soft key choose, the
 * right soft key closes; a touch chooses, a touch outside closes.
 */
final class CommandMenu {
    private final Command[] commands;
    private int selected;
    boolean closed;

    CommandMenu(Vector cmds) {
        commands = new Command[cmds.size()];
        cmds.copyInto(commands);
    }

    private static String label(Command c) {
        return c.getLongLabel() != null ? c.getLongLabel() : c.getLabel();
    }

    private int rowHeight() {
        return Skin.TEXT.getHeight() + 4;
    }

    private int top(int h) {
        return h - Skin.barHeight() - commands.length * rowHeight() - 2;
    }

    void paint(Graphics g, int w, int h) {
        int rh = rowHeight(), y = top(h);
        int mw = w * 3 / 4;
        g.setColor(Skin.BORDER);
        g.drawRect(0, y - 1, mw, commands.length * rh + 2);
        g.setFont(Skin.TEXT);
        for (int i = 0; i < commands.length; i++) {
            boolean on = i == selected;
            g.setColor(on ? Skin.HIGHLIGHT : Skin.FIELD);
            g.fillRect(1, y + i * rh, mw - 1, rh);
            g.setColor(on ? Skin.HIGHLIGHT_TEXT : Skin.FOREGROUND);
            g.drawString(Skin.fit(label(commands[i]), Skin.TEXT, mw - 10), 6, y + i * rh + 2, Graphics.TOP | Graphics.LEFT);
        }
        Skin.softBar(g, "Select", "Cancel", w, h);
    }

    /** A key: the command chosen, or null. */
    Command key(int code) {
        int action = Canvas.actionOf(code);
        if (code == Canvas.KEY_SOFT_RIGHT) {
            closed = true;
        } else if (action == Canvas.UP) {
            selected = (selected + commands.length - 1) % commands.length;
        } else if (action == Canvas.DOWN) {
            selected = (selected + 1) % commands.length;
        } else if (action == Canvas.FIRE || code == Canvas.KEY_SOFT_LEFT) {
            closed = true;
            return commands[selected];
        }
        return null;
    }

    /** A touch: the command chosen, or null. */
    Command pointer(int type, int x, int y, int w, int h) {
        int rh = rowHeight(), top = top(h);
        int i = y >= top && x < w * 3 / 4 ? (y - top) / rh : -1;
        boolean inside = i >= 0 && i < commands.length;
        if (inside) {
            selected = i;
        }
        if (type != Events.POINTER_UP) {
            return null;
        }
        if (y >= h - Skin.barHeight()) {   /* the bar: Select or Cancel */
            closed = true;
            return x < w / 2 ? commands[selected] : null;
        }
        closed = true;
        return inside ? commands[selected] : null;
    }
}
