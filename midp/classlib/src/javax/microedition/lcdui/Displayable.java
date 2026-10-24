package javax.microedition.lcdui;

import java.util.Vector;

/**
 * Something that can be shown on the display: a canvas or a screen, with a
 * title, a ticker and commands.
 */
public abstract class Displayable {
    /* Key events inside lcdui: the platform's two, and repeats. */
    static final int KEY_DOWN = jmevm.midp.Events.KEY_DOWN;
    static final int KEY_UP = jmevm.midp.Events.KEY_UP;
    static final int KEY_REPEAT = 100;

    String title;
    Ticker ticker;
    final Vector commands = new Vector();
    CommandListener listener;
    boolean shown;

    Displayable() {
    }

    public void addCommand(Command cmd) {
        if (cmd == null) {
            throw new NullPointerException();
        }
        synchronized (Display.LOCK) {
            if (!commands.contains(cmd)) {
                commands.addElement(cmd);
            }
        }
        Display.commandsChanged(this);
    }

    public void removeCommand(Command cmd) {
        synchronized (Display.LOCK) {
            commands.removeElement(cmd);
        }
        Display.commandsChanged(this);
    }

    public void setCommandListener(CommandListener l) {
        listener = l;
    }

    public String getTitle() {
        return title;
    }

    public void setTitle(String s) {
        title = s;
        Display.repaintIfShown(this);
    }

    public Ticker getTicker() {
        return ticker;
    }

    public void setTicker(Ticker t) {
        ticker = t;
        Display.repaintIfShown(this);
    }

    public boolean isShown() {
        return shown;
    }

    public int getWidth() {
        return Display.width;
    }

    public int getHeight() {
        return Display.height;
    }

    protected void sizeChanged(int w, int h) {
    }

    // ---- for Display (all on the UI thread)

    /** Paints the whole screen (canvases paint with a clip instead). */
    abstract void paintScreen(Graphics g);

    /** A key went down, up or repeats (KEY_DOWN, KEY_UP, KEY_REPEAT). */
    void keyEvent(int type, int keyCode) {
    }

    /** A touch, in screen coordinates (types of jmevm.midp.Events). */
    void pointerEvent(int type, int x, int y) {
    }

    void showing(boolean showing) {
        shown = showing;
    }

    /** Whether the soft keys go to the displayable instead of commands. */
    boolean takesSoftKeys() {
        return false;
    }

    /** Commands of the focused item, offered before the displayable's own. */
    Vector itemCommands() {
        return null;
    }

    /** Invokes a command of the displayable (or of its focused item). */
    void fire(Command c) {
        CommandListener l = listener;
        if (l != null) {
            l.commandAction(c, this);
        }
    }

    /** When this displayable next wants timeStep() (ms, absolute), or -1. */
    long nextTime() {
        return -1;
    }

    void timeStep(long now) {
    }
}
