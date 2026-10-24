package javax.microedition.lcdui;

/**
 * A message shown for a while (or until dismissed), then followed by the
 * displayable that was current or was given with it. Without commands of
 * its own it has DISMISS_COMMAND, and its timeout applies; with them it
 * waits for one.
 */
public class Alert extends Screen {
    public static final int FOREVER = -2;
    public static final Command DISMISS_COMMAND = new Command("Done", Command.OK, 0);

    private static final int DEFAULT_TIMEOUT = 2000;

    private String text;
    private Image image;
    private AlertType type;
    private int timeout = DEFAULT_TIMEOUT;
    private Gauge indicator;
    private long shownAt = -1;
    private final CommandListener dismiss = new CommandListener() {
        public void commandAction(Command c, Displayable d) {
            done();
        }
    };

    public Alert(String title) {
        this(title, null, null, null);
    }

    public Alert(String title, String alertText, Image alertImage, AlertType alertType) {
        this.title = title;
        this.text = alertText;
        this.image = alertImage;
        this.type = alertType;
        this.listener = dismiss;
    }

    public int getDefaultTimeout() {
        return DEFAULT_TIMEOUT;
    }

    public int getTimeout() {
        return timeout;
    }

    public void setTimeout(int time) {
        if (time <= 0 && time != FOREVER) {
            throw new IllegalArgumentException("timeout " + time);
        }
        timeout = time;
    }

    public AlertType getType() {
        return type;
    }

    public void setType(AlertType type) {
        this.type = type;
    }

    public String getString() {
        return text;
    }

    public void setString(String str) {
        text = str;
        Display.repaintIfShown(this);
    }

    public Image getImage() {
        return image;
    }

    public void setImage(Image img) {
        image = img;
        Display.repaintIfShown(this);
    }

    public void setIndicator(Gauge g) {
        if (g != null && (g.isInteractive() || g.owner != null || g.getLabel() != null || g.commands.size() > 0)) {
            throw new IllegalArgumentException("the indicator must be a non-interactive gauge of its own");
        }
        if (indicator != null) {
            indicator.owner = null;
        }
        indicator = g;
        if (g != null) {
            g.owner = this;
        }
        Display.repaintIfShown(this);
    }

    public Gauge getIndicator() {
        return indicator;
    }

    public void addCommand(Command cmd) {
        if (cmd == DISMISS_COMMAND) {
            return;
        }
        super.addCommand(cmd);
    }

    public void removeCommand(Command cmd) {
        if (cmd != DISMISS_COMMAND) {
            super.removeCommand(cmd);
        }
    }

    public void setCommandListener(CommandListener l) {
        super.setCommandListener(l != null ? l : dismiss);
    }

    // ---- showing

    private boolean timed() {
        return timeout != FOREVER && commands.size() == 0;
    }

    void shownAt(long now) {
        shownAt = now;
        if (type != null) {
            Display d = Display.current();
            if (d != null) {
                type.playSound(d);
            }
        }
    }

    private void done() {
        Display d = Display.current();
        if (d != null) {
            d.alertDone(this);
            jmevm.midp.Events.wakeup();
        }
    }

    /** Without a listener of its own, any command dismisses the alert;
     *  with one, the listener decides (DISMISS_COMMAND included). */
    void fire(Command c) {
        if (listener == dismiss) {
            done();
        } else {
            super.fire(c);
        }
    }

    java.util.Vector itemCommands() {
        if (commands.size() > 0) {
            return null;
        }
        java.util.Vector v = new java.util.Vector();
        v.addElement(DISMISS_COMMAND);
        return v;
    }

    long nextTime() {
        long t = super.nextTime();
        if (timed() && shownAt >= 0) {
            long end = shownAt + timeout;
            t = t < 0 ? end : Math.min(t, end);
        }
        return t;
    }

    void timeStep(long now) {
        if (super.nextTime() >= 0 && now >= super.nextTime()) {
            super.timeStep(now);
        }
        if (timed() && shownAt >= 0 && now >= shownAt + timeout) {
            shownAt = -1;
            done();
        }
    }

    // ---- painting

    private String[] lines(int width) {
        return text == null ? new String[0] : TextLines.wrap(text, Skin.TEXT, width - 2 * Skin.GAP);
    }

    int contentHeight(int width) {
        int h = Skin.GAP;
        if (image != null) {
            h += image.getHeight() + Skin.GAP;
        }
        h += lines(width).length * Skin.TEXT.getHeight() + Skin.GAP;
        if (indicator != null) {
            h += indicator.contentHeight(width) + Skin.GAP;
        }
        return h;
    }

    void paintContent(Graphics g, int width, int height) {
        int y = Skin.GAP;
        if (image != null) {
            g.drawImage(image, width / 2, y, Graphics.TOP | Graphics.HCENTER);
            y += image.getHeight() + Skin.GAP;
        }
        g.setColor(Skin.FOREGROUND);
        g.setFont(Skin.TEXT);
        String[] ls = lines(width);
        for (int i = 0; i < ls.length; i++) {
            g.drawString(ls[i], width / 2, y, Graphics.TOP | Graphics.HCENTER);
            y += Skin.TEXT.getHeight();
        }
        if (indicator != null) {
            y += Skin.GAP;
            g.translate(Skin.GAP, y);
            indicator.paintContent(g, width - 2 * Skin.GAP, indicator.contentHeight(width), false);
            g.translate(-Skin.GAP, -y);
        }
    }

    void keyEvent(int type, int keyCode) {
        if (type == KEY_DOWN && Canvas.actionOf(keyCode) == Canvas.FIRE && commands.size() == 0) {
            done();
            return;
        }
        super.keyEvent(type, keyCode);
    }
}
