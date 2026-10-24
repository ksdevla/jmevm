package javax.microedition.lcdui.game;

import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Graphics;
import javax.microedition.lcdui.Image;

import jmevm.midp.LcduiAccess;

/**
 * A canvas with its own frame buffer, drawn in the game's thread and shown
 * with flushGraphics(), and key states read when the game wants them.
 */
public abstract class GameCanvas extends Canvas {
    public static final int UP_PRESSED = 1 << UP;
    public static final int DOWN_PRESSED = 1 << DOWN;
    public static final int LEFT_PRESSED = 1 << LEFT;
    public static final int RIGHT_PRESSED = 1 << RIGHT;
    public static final int FIRE_PRESSED = 1 << FIRE;
    public static final int GAME_A_PRESSED = 1 << GAME_A;
    public static final int GAME_B_PRESSED = 1 << GAME_B;
    public static final int GAME_C_PRESSED = 1 << GAME_C;
    public static final int GAME_D_PRESSED = 1 << GAME_D;

    private final Image buffer;
    private int held;      /* the game-action keys that are down */
    private int latched;   /* those pressed since the last getKeyStates() */

    protected GameCanvas(boolean suppressKeyEvents) {
        buffer = Image.createImage(getWidth(), getHeight());
        LcduiAccess.get().setGameKeys(this, new LcduiAccess.GameKeys() {
            public void keyChanged(int gameAction, boolean down) {
                int bit = 1 << gameAction;
                synchronized (GameCanvas.this) {
                    if (down) {
                        held |= bit;
                        latched |= bit;
                    } else {
                        held &= ~bit;
                    }
                }
            }
        }, suppressKeyEvents);
    }

    /** The keys down now, and those pressed since the last call. */
    public int getKeyStates() {
        synchronized (this) {
            int states = held | latched;
            latched = 0;
            return states;
        }
    }

    protected Graphics getGraphics() {
        return buffer.getGraphics();
    }

    public void paint(Graphics g) {
        g.drawImage(buffer, 0, 0, Graphics.TOP | Graphics.LEFT);
    }

    public void flushGraphics(int x, int y, int width, int height) {
        if (width > 0 && height > 0) {
            LcduiAccess.get().flush(this, buffer, x, y, width, height);
        }
    }

    public void flushGraphics() {
        LcduiAccess.get().flush(this, buffer, 0, 0, buffer.getWidth(), buffer.getHeight());
    }
}
