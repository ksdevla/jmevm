package javax.microedition.lcdui;

import jmevm.midp.Events;

/**
 * An item the application draws and controls itself. It gets the keys and
 * touches while focused; up and down leave it unless traverse() keeps them.
 */
public abstract class CustomItem extends Item {
    protected static final int TRAVERSE_HORIZONTAL = 1;
    protected static final int TRAVERSE_VERTICAL = 2;
    protected static final int KEY_PRESS = 4;
    protected static final int KEY_RELEASE = 8;
    protected static final int KEY_REPEAT = 0x10;
    protected static final int POINTER_PRESS = 0x20;
    protected static final int POINTER_RELEASE = 0x40;
    protected static final int POINTER_DRAG = 0x80;
    protected static final int NONE = 0;

    private boolean inside;   /* traverse() has accepted the focus */

    protected CustomItem(String label) {
        super(label);
    }

    public int getGameAction(int keyCode) {
        if (keyCode == 0) {
            throw new IllegalArgumentException("key code 0");
        }
        return Canvas.actionOf(keyCode);
    }

    protected final int getInteractionModes() {
        return TRAVERSE_HORIZONTAL | TRAVERSE_VERTICAL | KEY_PRESS | KEY_RELEASE | KEY_REPEAT | POINTER_PRESS
                | POINTER_RELEASE | POINTER_DRAG;
    }

    protected abstract int getMinContentWidth();

    protected abstract int getMinContentHeight();

    protected abstract int getPrefContentWidth(int height);

    protected abstract int getPrefContentHeight(int width);

    protected abstract void paint(Graphics g, int w, int h);

    protected void sizeChanged(int w, int h) {
    }

    protected final void invalidate() {
        changed();
    }

    protected final void repaint() {
        changed();
    }

    protected final void repaint(int x, int y, int w, int h) {
        changed();
    }

    protected void keyPressed(int keyCode) {
    }

    protected void keyReleased(int keyCode) {
    }

    protected void keyRepeated(int keyCode) {
    }

    protected void pointerPressed(int x, int y) {
    }

    protected void pointerReleased(int x, int y) {
    }

    protected void pointerDragged(int x, int y) {
    }

    protected void showNotify() {
    }

    protected void hideNotify() {
    }

    protected boolean traverse(int dir, int viewportWidth, int viewportHeight, int[] visRect_inout) {
        return false;
    }

    protected void traverseOut() {
    }

    // ---- on a form

    boolean focusable() {
        return true;
    }

    int contentHeight(int width) {
        return Math.max(getMinContentHeight(), getPrefContentHeight(width));
    }

    void paintContent(Graphics g, int width, int height, boolean focused) {
        int w = Math.min(width, Math.max(getMinContentWidth(), getPrefContentWidth(height)));
        int clipX = g.getClipX(), clipY = g.getClipY(), clipW = g.getClipWidth(), clipH = g.getClipHeight();
        g.clipRect(0, 0, w, height);
        g.setColor(0);
        g.setFont(Font.getDefaultFont());
        g.setStrokeStyle(Graphics.SOLID);
        paint(g, w, height);
        g.setClip(clipX, clipY, clipW, clipH);
    }

    void focusChanged(boolean focused) {
        if (focused) {
            inside = traverse(Canvas.DOWN, Display.width, Display.height, new int[4]);
        } else {
            inside = false;
            traverseOut();
        }
    }

    boolean key(int type, int keyCode) {
        int action = Canvas.actionOf(keyCode);
        if ((action == Canvas.UP || action == Canvas.DOWN) && type == Displayable.KEY_DOWN) {
            if (!traverse(action, Display.width, Display.height, new int[4])) {
                if (inside) {
                    inside = false;
                    traverseOut();
                }
                return false;
            }
            inside = true;
            changed();
            return true;
        }
        if (type == Displayable.KEY_DOWN) {
            keyPressed(keyCode);
        } else if (type == Displayable.KEY_UP) {
            keyReleased(keyCode);
        } else {
            keyRepeated(keyCode);
        }
        return true;
    }

    void pointer(int type, int x, int y, int width, int height) {
        if (type == Events.POINTER_DOWN) {
            pointerPressed(x, y);
        } else if (type == Events.POINTER_UP) {
            pointerReleased(x, y);
        } else {
            pointerDragged(x, y);
        }
    }
}
