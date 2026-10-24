package javax.microedition.lcdui;

import java.util.Vector;

import javax.microedition.midlet.MIDlet;

import jmevm.midp.Events;
import jmevm.midp.LcduiAccess;

/**
 * The display, and the UI thread behind it.
 *
 * <p>Every callback of the UI (paint, keys, touches, commands, item changes,
 * callSerially) runs on one thread, in order. Other threads only record
 * what they want (a new displayable, a repaint, a task) under LOCK and wake
 * the UI thread, which switches displayables, paints into the back buffer
 * and presents it, runs tasks, and then waits for input or for the next
 * timer (key repeat, alert timeout, ticker).
 */
public class Display {
    public static final int LIST_ELEMENT = 1;
    public static final int CHOICE_GROUP_ELEMENT = 2;
    public static final int ALERT = 3;
    public static final int COLOR_BACKGROUND = 0;
    public static final int COLOR_FOREGROUND = 1;
    public static final int COLOR_HIGHLIGHTED_BACKGROUND = 2;
    public static final int COLOR_HIGHLIGHTED_FOREGROUND = 3;
    public static final int COLOR_BORDER = 4;
    public static final int COLOR_HIGHLIGHTED_BORDER = 5;

    private static final int REPEAT_DELAY = 500;
    private static final int REPEAT_EVERY = 100;

    /** Guards the state shared with other threads. */
    static final Object LOCK = new Object();
    /** Guards the back buffer and presenting it. */
    static final Object SCREEN = new Object();

    static final int width = jmevm.midp.Screen.width();
    static final int height = jmevm.midp.Screen.height();

    private static Display display;

    private final Image frame = Image.createImage(width, height);
    private final Graphics frameGraphics = new Graphics(frame, true);
    private Thread thread;

    /* Shared, under LOCK. */
    private Displayable current;
    private Displayable requested;
    private Displayable afterAlert;
    private boolean dirty;
    private int dx1, dy1, dx2, dy2;   /* the region to repaint */
    private int paints;
    private final Vector tasks = new Vector();

    /* UI thread only. */
    private CommandMenu menu;
    private int repeatKey;
    private long repeatAt = -1;
    private String shownLeft, shownRight;

    static {
        LcduiAccess.install(new Access());
    }

    private Display() {
    }

    public static Display getDisplay(MIDlet m) {
        if (m == null) {
            throw new NullPointerException();
        }
        return get();
    }

    private static Display get() {
        synchronized (LOCK) {
            if (display == null) {
                display = new Display();
                display.thread = new Thread() {
                    public void run() {
                        display.loop();
                    }
                };
                display.thread.start();
            }
            return display;
        }
    }

    // ---- the public API

    public int getColor(int colorSpecifier) {
        switch (colorSpecifier) {
        case COLOR_BACKGROUND: return Skin.BACKGROUND;
        case COLOR_FOREGROUND: return Skin.FOREGROUND;
        case COLOR_HIGHLIGHTED_BACKGROUND: return Skin.HIGHLIGHT;
        case COLOR_HIGHLIGHTED_FOREGROUND: return Skin.HIGHLIGHT_TEXT;
        case COLOR_BORDER: return Skin.BORDER;
        case COLOR_HIGHLIGHTED_BORDER: return Skin.HIGHLIGHT;
        default: throw new IllegalArgumentException("colour specifier " + colorSpecifier);
        }
    }

    public int getBorderStyle(boolean highlighted) {
        return Graphics.SOLID;
    }

    public boolean isColor() {
        return true;
    }

    public int numColors() {
        return 1 << 24;
    }

    public int numAlphaLevels() {
        return 256;
    }

    public int getBestImageWidth(int imageType) {
        checkImageType(imageType);
        return imageType == ALERT ? width : Skin.TEXT.getHeight();
    }

    public int getBestImageHeight(int imageType) {
        checkImageType(imageType);
        return imageType == ALERT ? height / 3 : Skin.TEXT.getHeight();
    }

    private static void checkImageType(int t) {
        if (t != LIST_ELEMENT && t != CHOICE_GROUP_ELEMENT && t != ALERT) {
            throw new IllegalArgumentException("image type " + t);
        }
    }

    public boolean flashBacklight(int duration) {
        if (duration < 0) {
            throw new IllegalArgumentException("duration " + duration);
        }
        return false;
    }

    public boolean vibrate(int duration) {
        if (duration < 0) {
            throw new IllegalArgumentException("duration " + duration);
        }
        return false;
    }

    public Displayable getCurrent() {
        synchronized (LOCK) {
            return requested != null ? requested : current;
        }
    }

    public void setCurrent(Displayable next) {
        if (next == null) {
            return;   /* would send the MIDlet to the background: nothing here */
        }
        synchronized (LOCK) {
            if (next instanceof Alert) {
                afterAlert = requested != null ? requested : current;
            }
            requested = next;
        }
        Events.wakeup();
    }

    public void setCurrent(Alert alert, Displayable nextDisplayable) {
        if (alert == null || nextDisplayable == null) {
            throw new NullPointerException();
        }
        if (nextDisplayable instanceof Alert) {
            throw new IllegalArgumentException("an alert cannot follow an alert");
        }
        synchronized (LOCK) {
            afterAlert = nextDisplayable;
            requested = alert;
        }
        Events.wakeup();
    }

    public void setCurrentItem(Item item) {
        Screen owner = item.owner;
        if (owner == null || owner instanceof Alert) {
            throw new IllegalStateException("the item is on no form");
        }
        if (owner instanceof Form) {
            ((Form) owner).focus(item);
        }
        setCurrent(owner);
    }

    public void callSerially(Runnable r) {
        synchronized (LOCK) {
            tasks.addElement(r);
        }
        Events.wakeup();
    }

    // ---- requests from any thread

    static void repaint(Canvas c, int x, int y, int w, int h) {
        Display d = display;
        if (d == null || w <= 0 || h <= 0) {
            return;
        }
        synchronized (LOCK) {
            if (d.current != c && d.requested != c) {
                return;
            }
            d.addDirty(x, y, x + w, y + h);
        }
        Events.wakeup();
    }

    /** Repaints the current displayable if it is d. */
    static void repaintIfShown(Displayable d) {
        Display disp = display;
        if (disp != null && disp.current == d) {
            repaintCurrent();
        }
    }

    static void repaintCurrent() {
        Display d = display;
        if (d == null) {
            return;
        }
        synchronized (LOCK) {
            d.addDirty(0, 0, width, height);
        }
        Events.wakeup();
    }

    static void commandsChanged(Displayable d) {
        repaintIfShown(d);
    }

    private void addDirty(int x1, int y1, int x2, int y2) {
        x1 = Math.max(0, x1);
        y1 = Math.max(0, y1);
        x2 = Math.min(width, x2);
        y2 = Math.min(height, y2);
        if (x1 >= x2 || y1 >= y2) {
            return;
        }
        if (!dirty) {
            dx1 = x1;
            dy1 = y1;
            dx2 = x2;
            dy2 = y2;
            dirty = true;
        } else {
            dx1 = Math.min(dx1, x1);
            dy1 = Math.min(dy1, y1);
            dx2 = Math.max(dx2, x2);
            dy2 = Math.max(dy2, y2);
        }
    }

    /** Returns once the canvas's pending repaint (if any) is painted. */
    static void serviceRepaints(Canvas c) {
        Display d = display;
        if (d == null) {
            return;
        }
        if (Thread.currentThread() == d.thread) {
            d.paintIfDirty();
            return;
        }
        synchronized (LOCK) {
            int target = d.paints + 1;
            while (d.dirty && d.current == c && d.paints < target) {
                try {
                    LOCK.wait(1000);
                } catch (InterruptedException e) {
                    return;
                }
            }
        }
    }

    /** GameCanvas.flushGraphics: the buffer's region onto the screen, now. */
    static void flush(Canvas c, Image buffer, int x, int y, int w, int h) {
        Display d = display;
        if (d == null || d.current != c) {
            return;
        }
        synchronized (SCREEN) {
            Graphics g = d.frameGraphics;
            g.reset(x, y, w, h);
            g.drawRegion0(buffer, 0, 0, Math.min(buffer.width, width), Math.min(buffer.height, height), 0, 0, 0);
            if (d.menu != null) {
                g.reset(0, 0, width, height);
                d.menu.paint(g, width, height);
            }
            jmevm.midp.Screen.present(d.frame);
        }
    }

    // ---- the UI thread

    private void loop() {
        int[] event = new int[2];
        for (;;) {
            switchDisplayable();
            paintIfDirty();
            runTasks();
            long now = System.currentTimeMillis();
            timers(now);
            long wake = nextTimer();
            int timeout = wake < 0 ? -1 : (int) Math.max(0, Math.min(Integer.MAX_VALUE, wake - now));
            int type = Events.next(event, timeout);
            if (type != 0) {
                dispatch(type, event[0], event[1]);
            }
        }
    }

    private void switchDisplayable() {
        Displayable old, next;
        synchronized (LOCK) {
            next = requested;
            if (next == null || next == current) {
                requested = null;
                return;
            }
            requested = null;
            old = current;
            current = next;
            addDirty(0, 0, width, height);
        }
        menu = null;
        repeatAt = -1;
        try {
            if (old != null) {
                old.showing(false);
            }
            if (next instanceof Alert) {
                ((Alert) next).shownAt(System.currentTimeMillis());
            }
            next.showing(true);
        } catch (Throwable t) {
            t.printStackTrace();
        }
        updateSoftLabels();
    }

    /** What follows an alert that is done. */
    void alertDone(Alert a) {
        synchronized (LOCK) {
            if (current != a || requested != null) {
                return;
            }
            requested = afterAlert != null ? afterAlert : a;
        }
    }

    private void paintIfDirty() {
        Displayable d;
        int x1, y1, x2, y2;
        synchronized (LOCK) {
            if (!dirty || current == null) {
                return;
            }
            d = current;
            dirty = false;
            x1 = dx1;
            y1 = dy1;
            x2 = dx2;
            y2 = dy2;
        }
        synchronized (SCREEN) {
            Graphics g = frameGraphics;
            if (d instanceof Canvas) {
                g.reset(x1, y1, x2 - x1, y2 - y1);
            } else {
                g.reset(0, 0, width, height);
            }
            try {
                d.paintScreen(g);
            } catch (Throwable t) {
                t.printStackTrace();
            }
            if (menu != null) {
                g.reset(0, 0, width, height);
                menu.paint(g, width, height);
            }
            jmevm.midp.Screen.present(frame);
        }
        synchronized (LOCK) {
            paints++;
            LOCK.notifyAll();
        }
        updateSoftLabels();
    }

    private void runTasks() {
        Runnable[] batch;
        synchronized (LOCK) {
            if (tasks.size() == 0) {
                return;
            }
            batch = new Runnable[tasks.size()];
            tasks.copyInto(batch);
            tasks.removeAllElements();
        }
        for (int i = 0; i < batch.length; i++) {
            try {
                batch[i].run();
            } catch (Throwable t) {
                t.printStackTrace();
            }
        }
    }

    private long nextTimer() {
        long wake = repeatAt;
        Displayable d = current;
        long t = d == null ? -1 : d.nextTime();
        if (t >= 0 && (wake < 0 || t < wake)) {
            wake = t;
        }
        synchronized (LOCK) {
            if (tasks.size() > 0 || dirty || requested != null) {
                return System.currentTimeMillis();
            }
        }
        return wake;
    }

    private void timers(long now) {
        Displayable d = current;
        if (d == null) {
            return;
        }
        if (repeatAt >= 0 && now >= repeatAt) {
            repeatAt = now + REPEAT_EVERY;
            deliverKey(d, Displayable.KEY_REPEAT, repeatKey);
        }
        long t = d.nextTime();
        if (t >= 0 && now >= t) {
            try {
                d.timeStep(now);
            } catch (Throwable e) {
                e.printStackTrace();
            }
        }
    }

    private void dispatch(int type, int a, int b) {
        Displayable d = current;
        if (d == null) {
            return;
        }
        try {
            if (type == Events.KEY_DOWN || type == Events.KEY_UP) {
                key(d, type, a);
            } else {
                pointer(d, type, a, b);
            }
        } catch (Throwable t) {
            t.printStackTrace();
        }
    }

    private void key(Displayable d, int type, int code) {
        if (type == Events.KEY_DOWN) {
            repeatKey = code;
            repeatAt = System.currentTimeMillis() + REPEAT_DELAY;
        } else if (code == repeatKey) {
            repeatAt = -1;
        }
        if (menu != null) {
            if (type == Events.KEY_DOWN) {
                menuKey(d, code);
            }
            return;
        }
        boolean soft = code == Canvas.KEY_SOFT_LEFT || code == Canvas.KEY_SOFT_RIGHT;
        if (soft && !d.takesSoftKeys()) {
            if (type == Events.KEY_DOWN) {
                softKey(d, code == Canvas.KEY_SOFT_LEFT);
            }
            return;
        }
        deliverKey(d, type, code);
    }

    private void deliverKey(Displayable d, int type, int code) {
        if (menu != null) {
            return;
        }
        if (type == Displayable.KEY_REPEAT && (code == Canvas.KEY_SOFT_LEFT || code == Canvas.KEY_SOFT_RIGHT)
                && !d.takesSoftKeys()) {
            return;
        }
        d.keyEvent(type, code);
    }

    private void pointer(Displayable d, int type, int x, int y) {
        if (menu != null) {
            Command c = menu.pointer(type, x, y, width, height);
            closeMenuIfDone(d, c);
            return;
        }
        if (!(d instanceof Canvas) && y >= height - Skin.barHeight()) {
            if (type == Events.POINTER_UP) {
                softKey(d, x < width / 2);
            }
            return;
        }
        d.pointerEvent(type, x, y);
    }

    // ---- commands on the soft keys

    /**
     * The commands of each soft key, {left, right}: the right key gets the
     * first command that leaves (Back, Cancel, Exit, Stop), or the second
     * of exactly two; the left key gets the rest. A key with more than one
     * opens a menu. The focused item's commands come first.
     */
    static Vector[] softKeys(Displayable d) {
        Vector left = new Vector(), right = new Vector();
        Vector items = d.itemCommands();
        Vector all = new Vector();
        for (int i = 0; items != null && i < items.size(); i++) {
            all.addElement(items.elementAt(i));
        }
        synchronized (LOCK) {
            for (int i = 0; i < d.commands.size(); i++) {
                all.addElement(d.commands.elementAt(i));
            }
        }
        for (int i = 0; i < all.size(); i++) {
            Command c = (Command) all.elementAt(i);
            if (right.size() == 0 && c.leaves()) {
                right.addElement(c);
            } else {
                left.addElement(c);
            }
        }
        if (right.size() == 0 && left.size() == 2) {
            right.addElement(left.elementAt(1));
            left.removeElementAt(1);
        }
        return new Vector[] {left, right};
    }

    /** The labels of the soft keys: a command's, "Menu", or null. */
    static String[] softLabels(Displayable d) {
        Vector[] keys = softKeys(d);
        String[] labels = new String[2];
        for (int i = 0; i < 2; i++) {
            if (keys[i].size() == 1) {
                labels[i] = ((Command) keys[i].elementAt(0)).getLabel();
            } else if (keys[i].size() > 1) {
                labels[i] = "Menu";
            }
        }
        return labels;
    }

    private void softKey(Displayable d, boolean left) {
        Vector cmds = softKeys(d)[left ? 0 : 1];
        if (cmds.size() == 1) {
            d.fire((Command) cmds.elementAt(0));
        } else if (cmds.size() > 1) {
            menu = new CommandMenu(cmds);
            repaintCurrent();
        }
    }

    private void menuKey(Displayable d, int code) {
        Command c = menu.key(code);
        closeMenuIfDone(d, c);
    }

    private void closeMenuIfDone(Displayable d, Command chosen) {
        if (menu.closed) {
            menu = null;
        }
        repaintCurrent();
        if (chosen != null) {
            d.fire(chosen);
        }
    }

    private void updateSoftLabels() {
        Displayable d = current;
        String[] labels = d == null || d.takesSoftKeys() ? new String[2] : softLabels(d);
        if (!same(labels[0], shownLeft) || !same(labels[1], shownRight)) {
            shownLeft = labels[0];
            shownRight = labels[1];
            jmevm.midp.Screen.softLabels(shownLeft, shownRight);
        }
    }

    private static boolean same(String a, String b) {
        return a == null ? b == null : a.equals(b);
    }

    static Display current() {
        return display;
    }

    // ---- for the game canvas and Nokia UI

    private static final class Access extends LcduiAccess {
        public void setGameKeys(Canvas canvas, GameKeys keys, boolean suppress) {
            canvas.gameKeys = keys;
            canvas.suppressGameKeys = suppress;
        }

        public void flush(Canvas canvas, Image buffer, int x, int y, int w, int h) {
            get();
            Display.flush(canvas, buffer, x, y, w, h);
        }

        public Image target(Graphics g) {
            return g.target;
        }

        public Image createMutable(int w, int h, int argb) {
            Image img = Image.createImage(w, h);
            for (int i = 0; i < img.pixels.length; i++) {
                img.pixels[i] = argb;
            }
            img.alpha = (argb >>> 24) != 0xFF;
            return img;
        }
    }
}
