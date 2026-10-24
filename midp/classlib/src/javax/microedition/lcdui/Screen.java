package javax.microedition.lcdui;

import jmevm.midp.Events;

/**
 * The high-level displayables (Form, List, TextBox, Alert): a title bar and
 * ticker at the top, the soft-key bar at the bottom, and content between
 * them that scrolls.
 */
public abstract class Screen extends Displayable {
    private static final int TICKER_STEP = 3;
    private static final int TICKER_EVERY = 100;

    int scroll;          /* content pixels scrolled out at the top */
    private int tickerOffset;
    private long tickerAt;
    private int dragY = -1;

    Screen() {
    }

    int contentTop() {
        return (title != null ? Skin.titleHeight() : 0) + (ticker != null ? Skin.tickerHeight() : 0);
    }

    public int getHeight() {
        return Display.height - contentTop() - Skin.barHeight();
    }

    /** The content's full height, for scrolling. */
    abstract int contentHeight(int width);

    /** Paints the content: (0, 0) is its top before scrolling. */
    abstract void paintContent(Graphics g, int width, int height);

    void paintScreen(Graphics g) {
        int w = Display.width, h = Display.height, top = contentTop(), view = getHeight();
        g.setColor(Skin.BACKGROUND);
        g.fillRect(0, 0, w, h);
        int max = Math.max(0, contentHeight(w) - view);
        scroll = Math.max(0, Math.min(scroll, max));
        g.setClip(0, top, w, view);
        g.translate(0, top - scroll);
        paintContent(g, w, view);
        g.translate(0, scroll - top);
        g.setClip(0, 0, w, h);
        if (max > 0) {   /* scroll bar */
            int bar = Math.max(8, view * view / (max + view));
            g.setColor(Skin.BORDER);
            g.fillRect(w - 3, top + (view - bar) * scroll / max, 2, bar);
        }
        int y = 0;
        if (title != null) {
            Skin.title(g, title, y, w);
            y += Skin.titleHeight();
        }
        if (ticker != null) {
            Skin.ticker(g, ticker.getString(), tickerOffset, y, w);
        }
        String[] labels = Display.softLabels(this);
        Skin.softBar(g, labels[0], labels[1], w, h);
    }

    /** Scrolls so that content rows y..y+h are visible. */
    void reveal(int y, int h) {
        int view = getHeight();
        if (y < scroll) {
            scroll = y;
        } else if (y + h > scroll + view) {
            scroll = y + h - view;
        }
    }

    void scrollBy(int dy) {
        scroll = Math.max(0, Math.min(scroll + dy, contentHeight(Display.width) - getHeight()));
        Display.repaintIfShown(this);
    }

    /** Up and down scroll the content (subclasses use keys first). */
    void keyEvent(int type, int keyCode) {
        if (type == KEY_UP) {
            return;
        }
        int action = Canvas.actionOf(keyCode);
        if (action == Canvas.UP) {
            scrollBy(-Skin.TEXT.getHeight());
        } else if (action == Canvas.DOWN) {
            scrollBy(Skin.TEXT.getHeight());
        }
    }

    /** Dragging scrolls; subclasses take taps (contentPointer). */
    void pointerEvent(int type, int x, int y) {
        int top = contentTop();
        if (type == Events.POINTER_DOWN) {
            dragY = y;
            contentPointer(type, x, y - top + scroll);
        } else if (type == Events.POINTER_DRAG && dragY >= 0) {
            if (Math.abs(y - dragY) > 4) {
                scrollBy(dragY - y);
                dragY = y;
            }
        } else if (type == Events.POINTER_UP) {
            dragY = -1;
            contentPointer(type, x, y - top + scroll);
        }
    }

    /** A touch on the content, in content coordinates. */
    void contentPointer(int type, int x, int y) {
    }

    long nextTime() {
        return ticker == null ? -1 : tickerAt;
    }

    void timeStep(long now) {
        tickerAt = now + TICKER_EVERY;
        if (ticker != null) {
            tickerOffset += TICKER_STEP;
            if (tickerOffset > Display.width + Skin.TEXT.stringWidth(ticker.getString())) {
                tickerOffset = 0;
            }
            Display.repaintIfShown(this);
        }
    }
}
