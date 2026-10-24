package javax.microedition.lcdui;

/**
 * A bar showing a value from 0 to a maximum; interactive gauges are moved
 * with left and right (or a touch). A non-interactive gauge can have an
 * indefinite range, shown by one of four states.
 */
public class Gauge extends Item {
    public static final int INDEFINITE = -1;
    public static final int CONTINUOUS_IDLE = 0;
    public static final int INCREMENTAL_IDLE = 1;
    public static final int CONTINUOUS_RUNNING = 2;
    public static final int INCREMENTAL_UPDATING = 3;

    private final boolean interactive;
    private int max;
    private int value;

    public Gauge(String label, boolean interactive, int maxValue, int initialValue) {
        super(label);
        this.interactive = interactive;
        checkMax(maxValue);
        max = maxValue;
        setValue(initialValue);
    }

    private void checkMax(int maxValue) {
        if (maxValue == INDEFINITE ? interactive : maxValue <= 0) {
            throw new IllegalArgumentException("maximum " + maxValue);
        }
    }

    public void setValue(int v) {
        if (max == INDEFINITE) {
            if (v < CONTINUOUS_IDLE || v > INCREMENTAL_UPDATING) {
                throw new IllegalArgumentException("state " + v);
            }
            value = v;
        } else {
            value = Math.max(0, Math.min(v, max));
        }
        changed();
    }

    public int getValue() {
        return value;
    }

    public void setMaxValue(int maxValue) {
        checkMax(maxValue);
        boolean wasIndefinite = max == INDEFINITE;
        max = maxValue;
        if (maxValue == INDEFINITE) {
            value = wasIndefinite ? value : INCREMENTAL_IDLE;
        } else if (wasIndefinite) {
            value = 0;
        } else {
            value = Math.min(value, maxValue);
        }
        changed();
    }

    public int getMaxValue() {
        return max;
    }

    public boolean isInteractive() {
        return interactive;
    }

    boolean focusable() {
        return interactive || super.focusable();
    }

    int contentHeight(int width) {
        return 12;
    }

    void paintContent(Graphics g, int width, int height, boolean focused) {
        g.setColor(Skin.FIELD);
        g.fillRect(0, 2, width, height - 4);
        g.setColor(Skin.HIGHLIGHT);
        if (max == INDEFINITE) {
            if (value == CONTINUOUS_RUNNING || value == INCREMENTAL_UPDATING) {
                for (int x = 0; x < width; x += 12) {
                    g.fillRect(x, 2, 6, height - 4);
                }
            }
        } else {
            g.fillRect(0, 2, (int) ((long) width * value / max), height - 4);
        }
        g.setColor(Skin.BORDER);
        g.drawRect(0, 2, width - 1, height - 5);
    }

    boolean key(int type, int keyCode) {
        if (!interactive || type == Displayable.KEY_UP) {
            return super.key(type, keyCode);
        }
        int action = Canvas.actionOf(keyCode);
        if (action == Canvas.LEFT || action == Canvas.RIGHT) {
            int v = Math.max(0, Math.min(max, value + (action == Canvas.LEFT ? -1 : 1)));
            if (v != value) {
                value = v;
                userChanged();
            }
            return true;
        }
        return super.key(type, keyCode);
    }

    void pointer(int type, int x, int y, int width, int height) {
        if (!interactive) {
            super.pointer(type, x, y, width, height);
            return;
        }
        int v = Math.max(0, Math.min(max, (int) (((long) x * max + width / 2) / Math.max(1, width))));
        if (v != value) {
            value = v;
            userChanged();
        }
    }
}
