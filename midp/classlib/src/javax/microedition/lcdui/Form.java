package javax.microedition.lcdui;

import java.util.Vector;

import jmevm.midp.Events;

/**
 * Items, one under the other. Up and down move the focus between the items
 * that can take it (an item may use them first, as a choice group moving
 * inside itself); with nothing to focus they scroll. A touch focuses an
 * item and goes to it.
 */
public class Form extends Screen {
    private final Vector items = new Vector();
    private ItemStateListener stateListener;
    private int focus = -1;

    public Form(String title) {
        this.title = title;
    }

    public Form(String title, Item[] items) {
        this(title);
        if (items != null) {
            for (int i = 0; i < items.length; i++) {
                append(items[i]);
            }
        }
    }

    // ---- the items

    private void own(Item item) {
        if (item == null) {
            throw new NullPointerException();
        }
        if (item.owner != null) {
            throw new IllegalStateException("the item is on another screen");
        }
        item.owner = this;
    }

    public int append(Item item) {
        own(item);
        synchronized (Display.LOCK) {
            items.addElement(item);
        }
        changed();
        return items.size() - 1;
    }

    public int append(String str) {
        if (str == null) {
            throw new NullPointerException();
        }
        return append(new StringItem(null, str));
    }

    public int append(Image img) {
        if (img == null) {
            throw new NullPointerException();
        }
        return append(new ImageItem(null, img, Item.LAYOUT_DEFAULT, null));
    }

    public void insert(int itemNum, Item item) {
        if (itemNum < 0 || itemNum > items.size()) {
            throw new IndexOutOfBoundsException("item " + itemNum);
        }
        own(item);
        synchronized (Display.LOCK) {
            items.insertElementAt(item, itemNum);
            if (focus >= itemNum) {
                focus++;
            }
        }
        changed();
    }

    public void delete(int itemNum) {
        synchronized (Display.LOCK) {
            Item item = (Item) items.elementAt(itemNum);   /* or IndexOutOfBounds */
            item.owner = null;
            items.removeElementAt(itemNum);
            if (focus == itemNum) {
                focus = -1;
            } else if (focus > itemNum) {
                focus--;
            }
        }
        changed();
    }

    public void deleteAll() {
        synchronized (Display.LOCK) {
            for (int i = 0; i < items.size(); i++) {
                ((Item) items.elementAt(i)).owner = null;
            }
            items.removeAllElements();
            focus = -1;
        }
        changed();
    }

    public void set(int itemNum, Item item) {
        if (itemNum < 0 || itemNum >= items.size()) {
            throw new IndexOutOfBoundsException("item " + itemNum);
        }
        own(item);
        synchronized (Display.LOCK) {
            ((Item) items.elementAt(itemNum)).owner = null;
            items.setElementAt(item, itemNum);
        }
        changed();
    }

    public Item get(int itemNum) {
        if (itemNum < 0 || itemNum >= items.size()) {
            throw new IndexOutOfBoundsException("item " + itemNum);
        }
        return (Item) items.elementAt(itemNum);
    }

    public int size() {
        return items.size();
    }

    public void setItemStateListener(ItemStateListener l) {
        stateListener = l;
    }

    public int getWidth() {
        return Display.width;
    }

    private void changed() {
        Display.repaintIfShown(this);
    }

    void stateChanged(Item item) {
        ItemStateListener l = stateListener;
        if (l != null) {
            l.itemStateChanged(item);
        }
    }

    // ---- layout

    private int itemWidth() {
        return Display.width - 2 * Skin.GAP - 4;
    }

    /** The top of item i in the content, and the content's height for i = size. */
    private int top(int index) {
        int y = Skin.GAP, w = itemWidth();
        for (int i = 0; i < index && i < items.size(); i++) {
            y += ((Item) items.elementAt(i)).height(w) + Skin.GAP;
        }
        return y;
    }

    int contentHeight(int width) {
        return top(items.size());
    }

    void paintContent(Graphics g, int width, int height) {
        int w = itemWidth(), y = Skin.GAP;
        int clipY = g.getClipY(), clipH = g.getClipHeight();
        for (int i = 0; i < items.size(); i++) {
            Item item = (Item) items.elementAt(i);
            int h = item.height(w);
            if (y + h >= clipY && y <= clipY + clipH) {
                g.translate(Skin.GAP, y);
                item.paint(g, w, i == focus);
                if (i == focus) {
                    Skin.focus(g, -2, -2, w + 4, h + 4);
                }
                g.translate(-Skin.GAP, -y);
            }
            y += h + Skin.GAP;
        }
    }

    // ---- focus

    void focus(Item item) {
        int i = items.indexOf(item);
        if (i >= 0 && item.focusable()) {
            setFocus(i);
        }
    }

    private void setFocus(int i) {
        if (i == focus) {
            return;
        }
        if (focus >= 0 && focus < items.size()) {
            ((Item) items.elementAt(focus)).focusChanged(false);
        }
        focus = i;
        if (i >= 0) {
            Item item = (Item) items.elementAt(i);
            item.focusChanged(true);
            reveal(top(i) - Skin.GAP, item.height(itemWidth()) + 2 * Skin.GAP);
        }
        changed();
    }

    private Item focused() {
        return focus >= 0 && focus < items.size() ? (Item) items.elementAt(focus) : null;
    }

    /** The next focusable item from `from` in direction dir, or -1. */
    private int next(int from, int dir) {
        for (int i = from + dir; i >= 0 && i < items.size(); i += dir) {
            if (((Item) items.elementAt(i)).focusable()) {
                return i;
            }
        }
        return -1;
    }

    void showing(boolean showing) {
        super.showing(showing);
        if (showing && focus < 0) {
            int first = next(-1, 1);
            if (first >= 0) {
                setFocus(first);
            }
        }
    }

    Vector itemCommands() {
        Item item = focused();
        return item == null || item.commands.size() == 0 ? null : item.commands;
    }

    void fire(Command c) {
        Item item = focused();
        if (item != null && item.commands.contains(c)) {
            item.fireCommand(c);
        } else {
            super.fire(c);
        }
    }

    void keyEvent(int type, int keyCode) {
        Item item = focused();
        if (item != null && item.key(type, keyCode)) {
            return;
        }
        if (type == KEY_UP) {
            return;
        }
        int action = Canvas.actionOf(keyCode);
        if (action == Canvas.UP || action == Canvas.DOWN) {
            int dir = action == Canvas.UP ? -1 : 1;
            int n = next(focus, dir);
            if (n >= 0) {
                setFocus(n);
                return;
            }
        }
        super.keyEvent(type, keyCode);
    }

    void contentPointer(int type, int x, int y) {
        int w = itemWidth();
        for (int i = 0; i < items.size(); i++) {
            Item item = (Item) items.elementAt(i);
            int t = top(i), h = item.height(w);
            if (y >= t && y < t + h) {
                if (item.focusable()) {
                    if (type == Events.POINTER_DOWN) {
                        setFocus(i);
                    }
                    int lh = item.labelHeight(w);
                    item.pointer(type, x - Skin.GAP, y - t - lh, w, item.contentHeight(w));
                }
                return;
            }
        }
    }

    long nextTime() {
        long t = super.nextTime();
        for (int i = 0; i < items.size(); i++) {
            long it = ((Item) items.elementAt(i)).nextTime();
            if (it >= 0 && (t < 0 || it < t)) {
                t = it;
            }
        }
        return t;
    }

    void timeStep(long now) {
        if (super.nextTime() >= 0 && now >= super.nextTime()) {
            super.timeStep(now);
        }
        for (int i = 0; i < items.size(); i++) {
            Item item = (Item) items.elementAt(i);
            long t = item.nextTime();
            if (t >= 0 && now >= t) {
                item.timeStep(now);
            }
        }
    }
}
