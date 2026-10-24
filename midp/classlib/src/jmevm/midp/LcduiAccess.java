package jmevm.midp;

import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Graphics;
import javax.microedition.lcdui.Image;

/**
 * What javax.microedition.lcdui offers the classes of other packages that
 * need more than its public API: the game canvas (its own key states and
 * frame buffer) and the Nokia UI (the pixels behind a Graphics). lcdui
 * installs the one implementation when its Display class initializes.
 */
public abstract class LcduiAccess {
    /** Receives the game-action keys of a game canvas as they go down and up. */
    public interface GameKeys {
        void keyChanged(int gameAction, boolean down);
    }

    private static LcduiAccess instance;

    public static synchronized void install(LcduiAccess access) {
        if (instance == null) {
            instance = access;
        }
    }

    public static synchronized LcduiAccess get() {
        if (instance == null) {
            try {
                Class.forName("javax.microedition.lcdui.Display");   /* installs it */
            } catch (ClassNotFoundException e) {
                throw new IllegalStateException("no lcdui");
            }
        }
        return instance;
    }

    /** Routes a canvas's game keys to keys; with suppress, game-action keys
     *  no longer reach keyPressed() and the like. */
    public abstract void setGameKeys(Canvas canvas, GameKeys keys, boolean suppress);

    /** Shows a region of a game canvas's frame buffer, now (flushGraphics). */
    public abstract void flush(Canvas canvas, Image buffer, int x, int y, int width, int height);

    /** The image a Graphics draws on (the screen's back buffer for a canvas). */
    public abstract Image target(Graphics g);

    /** A mutable image filled with an ARGB colour, translucent if it is. */
    public abstract Image createMutable(int width, int height, int argb);
}
