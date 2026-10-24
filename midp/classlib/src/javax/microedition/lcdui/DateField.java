package javax.microedition.lcdui;

import java.util.Calendar;
import java.util.Date;
import java.util.TimeZone;

/**
 * A date, a time, or both, on a form. It starts without a value. When
 * focused, left and right step the value (a day, or a minute in TIME mode).
 */
public class DateField extends Item {
    public static final int DATE = 1;
    public static final int TIME = 2;
    public static final int DATE_TIME = 3;

    private static final long DAY = 24L * 60 * 60 * 1000;

    private int mode;
    private Date date;
    private final TimeZone zone;

    public DateField(String label, int mode) {
        this(label, mode, null);
    }

    public DateField(String label, int mode, TimeZone timeZone) {
        super(label);
        setInputMode(mode);
        zone = timeZone != null ? timeZone : TimeZone.getDefault();
    }

    public Date getDate() {
        return date == null ? null : new Date(date.getTime());
    }

    public void setDate(Date d) {
        date = d == null ? null : new Date(clip(d.getTime()));
        changed();
    }

    /** Keeps only what the mode shows: the day at midnight, or the time on 1 January 1970. */
    private long clip(long t) {
        long local = t + zone.getRawOffset();
        if (mode == DATE) {
            local -= ((local % DAY) + DAY) % DAY;
        } else if (mode == TIME) {
            local = ((local % DAY) + DAY) % DAY;
        }
        return local - zone.getRawOffset();
    }

    public int getInputMode() {
        return mode;
    }

    public void setInputMode(int mode) {
        if (mode != DATE && mode != TIME && mode != DATE_TIME) {
            throw new IllegalArgumentException("mode " + mode);
        }
        this.mode = mode;
        if (date != null) {
            date = new Date(clip(date.getTime()));
        }
        changed();
    }

    private static String two(int n) {
        return n < 10 ? "0" + n : String.valueOf(n);
    }

    private String text() {
        if (date == null) {
            return "<date>";
        }
        Calendar c = Calendar.getInstance(zone);
        c.setTime(date);
        String day = c.get(Calendar.YEAR) + "-" + two(c.get(Calendar.MONTH) + 1) + "-" + two(c.get(Calendar.DAY_OF_MONTH));
        String time = two(c.get(Calendar.HOUR_OF_DAY)) + ":" + two(c.get(Calendar.MINUTE));
        return mode == DATE ? day : mode == TIME ? time : day + " " + time;
    }

    boolean focusable() {
        return true;
    }

    int contentHeight(int width) {
        return Skin.TEXT.getHeight() + 4;
    }

    void paintContent(Graphics g, int width, int height, boolean focused) {
        g.setColor(Skin.FIELD);
        g.fillRect(0, 0, width, height);
        g.setColor(Skin.BORDER);
        g.drawRect(0, 0, width - 1, height - 1);
        g.setColor(date == null ? Skin.MUTED : Skin.FOREGROUND);
        g.setFont(Skin.TEXT);
        g.drawString(text(), 3, 2, Graphics.TOP | Graphics.LEFT);
    }

    boolean key(int type, int keyCode) {
        int action = Canvas.actionOf(keyCode);
        if (type != Displayable.KEY_UP && (action == Canvas.LEFT || action == Canvas.RIGHT)) {
            long step = mode == TIME ? 60 * 1000 : DAY;
            long base = date != null ? date.getTime() : clip(System.currentTimeMillis());
            date = new Date(clip(base + (action == Canvas.LEFT ? -step : step)));
            userChanged();
            return true;
        }
        return super.key(type, keyCode);
    }
}
