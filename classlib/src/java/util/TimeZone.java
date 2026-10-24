package java.util;

/**
 * A time zone: an offset from UTC, maybe with daylight saving time. The
 * platform knows GMT, UTC and fixed offsets written "GMT+hh:mm"; GMT is the
 * default, so the virtual machine's clock reads the same everywhere.
 */
public abstract class TimeZone {
    private static TimeZone defaultZone;

    private String id;

    public TimeZone() {
    }

    private TimeZone(String id) {
        this.id = id;
    }

    /**
     * The offset from UTC, in ms, of local standard time at a moment given
     * as local era, year, month (0 to 11), day of month, day of week and ms
     * into the day, daylight saving included.
     */
    public abstract int getOffset(int era, int year, int month, int day, int dayOfWeek, int millis);

    /** The offset from UTC, in ms, without daylight saving time. */
    public abstract int getRawOffset();

    public abstract boolean useDaylightTime();

    public String getID() {
        return id;
    }

    /** The zone of an ID; GMT for an ID the platform does not know. */
    public static synchronized TimeZone getTimeZone(String ID) {
        if (ID.equals("GMT") || ID.equals("UTC")) {
            return new Fixed(ID, 0);
        }
        TimeZone zone = parseOffset(ID);
        return zone != null ? zone : new Fixed("GMT", 0);
    }

    public static synchronized TimeZone getDefault() {
        if (defaultZone == null) {
            defaultZone = new Fixed("GMT", 0);
        }
        return defaultZone;
    }

    public static String[] getAvailableIDs() {
        return new String[] {"GMT", "UTC"};
    }

    /** "GMT" and a sign, then h, hh, hhmm or h[h]:mm; null if it is not that. */
    private static TimeZone parseOffset(String id) {
        if (!id.startsWith("GMT+") && !id.startsWith("GMT-")) {
            return null;
        }
        String digits = id.substring(4);
        int colon = digits.indexOf(':');
        String hh;
        String mm;
        if (colon >= 0) {
            hh = digits.substring(0, colon);
            mm = digits.substring(colon + 1);
            if (mm.length() != 2) {
                return null;
            }
        } else if (digits.length() <= 2) {
            hh = digits;
            mm = "00";
        } else if (digits.length() == 4) {
            hh = digits.substring(0, 2);
            mm = digits.substring(2);
        } else {
            return null;
        }
        int hours = number(hh);
        int minutes = number(mm);
        if (hh.length() == 0 || hh.length() > 2 || hours < 0 || hours > 23 || minutes < 0 || minutes > 59) {
            return null;
        }
        boolean negative = id.charAt(3) == '-';
        String normal = "GMT" + (negative ? '-' : '+') + (hours < 10 ? "0" : "") + hours + ':'
                + (minutes < 10 ? "0" : "") + minutes;
        int offset = (hours * 60 + minutes) * 60 * 1000;
        return new Fixed(normal, negative ? -offset : offset);
    }

    /** The value of ASCII digits, or -1. */
    private static int number(String s) {
        int v = 0;
        for (int i = 0; i < s.length(); i++) {
            char c = s.charAt(i);
            if (c < '0' || c > '9') {
                return -1;
            }
            v = v * 10 + (c - '0');
        }
        return v;
    }

    /** A zone at a constant offset, without daylight saving time. */
    private static final class Fixed extends TimeZone {
        private final int offset;

        Fixed(String id, int offset) {
            super(id);
            this.offset = offset;
        }

        public int getOffset(int era, int year, int month, int day, int dayOfWeek, int millis) {
            return offset;
        }

        public int getRawOffset() {
            return offset;
        }

        public boolean useDaylightTime() {
            return false;
        }
    }
}
