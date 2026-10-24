package javax.microedition.lcdui;

import jmevm.midp.Events;
import jmevm.midp.LcduiAccess;

/**
 * A displayable the application draws itself, with low-level key and touch
 * events. It covers the whole screen in both modes; its commands are on
 * the soft keys (which it receives as keys while it has no commands).
 *
 * <p>Key codes: the digits are '0'..'9', '*' and '#'; the arrows are -1 up,
 * -2 down, -3 left, -4 right; -5 is fire; -6 and -7 the left and right soft
 * keys; -8 clear. The game actions are on the arrows and fire, and on
 * 2, 4, 6, 8 and 5; GAME_A to GAME_D on 1, 3, 7 and 9.
 */
public abstract class Canvas extends Displayable {
    public static final int UP = 1;
    public static final int DOWN = 6;
    public static final int LEFT = 2;
    public static final int RIGHT = 5;
    public static final int FIRE = 8;
    public static final int GAME_A = 9;
    public static final int GAME_B = 10;
    public static final int GAME_C = 11;
    public static final int GAME_D = 12;
    public static final int KEY_NUM0 = 48;
    public static final int KEY_NUM1 = 49;
    public static final int KEY_NUM2 = 50;
    public static final int KEY_NUM3 = 51;
    public static final int KEY_NUM4 = 52;
    public static final int KEY_NUM5 = 53;
    public static final int KEY_NUM6 = 54;
    public static final int KEY_NUM7 = 55;
    public static final int KEY_NUM8 = 56;
    public static final int KEY_NUM9 = 57;
    public static final int KEY_STAR = 42;
    public static final int KEY_POUND = 35;

    static final int KEY_UP_ARROW = -1;
    static final int KEY_DOWN_ARROW = -2;
    static final int KEY_LEFT_ARROW = -3;
    static final int KEY_RIGHT_ARROW = -4;
    static final int KEY_SELECT = -5;
    static final int KEY_SOFT_LEFT = -6;
    static final int KEY_SOFT_RIGHT = -7;
    static final int KEY_CLEAR = -8;

    private boolean fullScreen;
    LcduiAccess.GameKeys gameKeys;   /* set by GameCanvas */
    boolean suppressGameKeys;

    protected Canvas() {
    }

    public boolean isDoubleBuffered() {
        return true;
    }

    public boolean hasPointerEvents() {
        return true;
    }

    public boolean hasPointerMotionEvents() {
        return true;
    }

    public boolean hasRepeatEvents() {
        return true;
    }

    // ---- keys

    /** The game action of a key code, or 0 for keys without one. */
    static int actionOf(int keyCode) {
        switch (keyCode) {
        case KEY_UP_ARROW: case KEY_NUM2: return UP;
        case KEY_DOWN_ARROW: case KEY_NUM8: return DOWN;
        case KEY_LEFT_ARROW: case KEY_NUM4: return LEFT;
        case KEY_RIGHT_ARROW: case KEY_NUM6: return RIGHT;
        case KEY_SELECT: case KEY_NUM5: return FIRE;
        case KEY_NUM1: return GAME_A;
        case KEY_NUM3: return GAME_B;
        case KEY_NUM7: return GAME_C;
        case KEY_NUM9: return GAME_D;
        default: return 0;
        }
    }

    public int getGameAction(int keyCode) {
        /* 0 is never a key code. Other codes without an action give 0, as on
         * phones: games ask about whatever keys they receive. */
        if (keyCode == 0) {
            throw new IllegalArgumentException("key code 0");
        }
        return actionOf(keyCode);
    }

    public int getKeyCode(int gameAction) {
        switch (gameAction) {
        case UP: return KEY_UP_ARROW;
        case DOWN: return KEY_DOWN_ARROW;
        case LEFT: return KEY_LEFT_ARROW;
        case RIGHT: return KEY_RIGHT_ARROW;
        case FIRE: return KEY_SELECT;
        case GAME_A: return KEY_NUM1;
        case GAME_B: return KEY_NUM3;
        case GAME_C: return KEY_NUM7;
        case GAME_D: return KEY_NUM9;
        default: throw new IllegalArgumentException("game action " + gameAction);
        }
    }

    public String getKeyName(int keyCode) {
        if (keyCode >= KEY_NUM0 && keyCode <= KEY_NUM9) {
            return String.valueOf((char) keyCode);
        }
        switch (keyCode) {
        case KEY_STAR: return "*";
        case KEY_POUND: return "#";
        case KEY_UP_ARROW: return "Up";
        case KEY_DOWN_ARROW: return "Down";
        case KEY_LEFT_ARROW: return "Left";
        case KEY_RIGHT_ARROW: return "Right";
        case KEY_SELECT: return "Select";
        case KEY_SOFT_LEFT: return "Left soft key";
        case KEY_SOFT_RIGHT: return "Right soft key";
        case KEY_CLEAR: return "Clear";
        default:
            if (keyCode == 0) {
                throw new IllegalArgumentException("key code 0");
            }
            return "Key " + keyCode;
        }
    }

    protected void keyPressed(int keyCode) {
    }

    protected void keyRepeated(int keyCode) {
    }

    protected void keyReleased(int keyCode) {
    }

    protected void pointerPressed(int x, int y) {
    }

    protected void pointerReleased(int x, int y) {
    }

    protected void pointerDragged(int x, int y) {
    }

    // ---- painting

    public final void repaint(int x, int y, int width, int height) {
        Display.repaint(this, x, y, width, height);
    }

    public final void repaint() {
        Display.repaint(this, 0, 0, Display.width, Display.height);
    }

    public final void serviceRepaints() {
        Display.serviceRepaints(this);
    }

    protected void showNotify() {
    }

    protected void hideNotify() {
    }

    protected abstract void paint(Graphics g);

    public void setFullScreenMode(boolean mode) {
        if (fullScreen != mode) {
            fullScreen = mode;
            repaint();
        }
    }

    // ---- for Display

    void paintScreen(Graphics g) {
        paint(g);
    }

    boolean takesSoftKeys() {
        return commands.size() == 0;
    }

    void showing(boolean showing) {
        super.showing(showing);
        if (showing) {
            showNotify();
        } else {
            hideNotify();
        }
    }

    void keyEvent(int type, int keyCode) {
        int action = actionOf(keyCode);
        LcduiAccess.GameKeys keys = gameKeys;
        if (keys != null && action != 0 && type != KEY_REPEAT) {
            keys.keyChanged(action, type == KEY_DOWN);
        }
        if (action != 0 && suppressGameKeys) {
            return;
        }
        if (type == KEY_DOWN) {
            keyPressed(keyCode);
        } else if (type == KEY_UP) {
            keyReleased(keyCode);
        } else {
            keyRepeated(keyCode);
        }
    }

    void pointerEvent(int type, int x, int y) {
        if (type == Events.POINTER_DOWN) {
            pointerPressed(x, y);
        } else if (type == Events.POINTER_UP) {
            pointerReleased(x, y);
        } else {
            pointerDragged(x, y);
        }
    }
}
