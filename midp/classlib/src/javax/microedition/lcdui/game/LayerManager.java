package javax.microedition.lcdui.game;

import java.util.Vector;

import javax.microedition.lcdui.Graphics;

/**
 * Layers in order, index 0 nearest the viewer, drawn through a view
 * window: the part of the layers' world that paint() shows.
 */
public class LayerManager {
    private final Vector layers = new Vector();
    private int viewX, viewY;
    private int viewW = Integer.MAX_VALUE, viewH = Integer.MAX_VALUE;

    public LayerManager() {
    }

    public void append(Layer l) {
        if (l == null) {
            throw new NullPointerException();
        }
        layers.removeElement(l);
        layers.addElement(l);
    }

    public void insert(Layer l, int index) {
        if (l == null) {
            throw new NullPointerException();
        }
        layers.removeElement(l);
        if (index < 0 || index > layers.size()) {
            throw new IndexOutOfBoundsException("index " + index);
        }
        layers.insertElementAt(l, index);
    }

    public Layer getLayerAt(int index) {
        if (index < 0 || index >= layers.size()) {
            throw new IndexOutOfBoundsException("index " + index);
        }
        return (Layer) layers.elementAt(index);
    }

    public int getSize() {
        return layers.size();
    }

    public void remove(Layer l) {
        if (l == null) {
            throw new NullPointerException();
        }
        layers.removeElement(l);
    }

    public void setViewWindow(int x, int y, int width, int height) {
        if (width < 0 || height < 0) {
            throw new IllegalArgumentException("view window " + width + "x" + height);
        }
        viewX = x;
        viewY = y;
        viewW = width;
        viewH = height;
    }

    /** Draws the view window with its top left at (x, y); the Graphics'
     *  clip and translation are as before afterwards. */
    public void paint(Graphics g, int x, int y) {
        int clipX = g.getClipX(), clipY = g.getClipY(), clipW = g.getClipWidth(), clipH = g.getClipHeight();
        int tx = g.getTranslateX(), ty = g.getTranslateY();
        g.clipRect(x, y, viewW, viewH);
        g.translate(x - viewX, y - viewY);
        for (int i = layers.size() - 1; i >= 0; i--) {
            Layer l = (Layer) layers.elementAt(i);
            if (l.isVisible()) {
                l.paint(g);
            }
        }
        g.translate(tx - g.getTranslateX(), ty - g.getTranslateY());
        g.setClip(clipX, clipY, clipW, clipH);
    }
}
