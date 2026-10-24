package java.util;

/** A moment in time, in ms since 1970-01-01 00:00:00 UTC. */
public class Date {
    private static final String[] DAYS = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    private static final String[] MONTHS = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct",
            "Nov", "Dec"};

    private long millis;

    /** Now. */
    public Date() {
        this(System.currentTimeMillis());
    }

    public Date(long date) {
        millis = date;
    }

    public long getTime() {
        return millis;
    }

    public void setTime(long time) {
        millis = time;
    }

    public boolean equals(Object obj) {
        return obj instanceof Date && ((Date) obj).millis == millis;
    }

    public int hashCode() {
        return (int) (millis ^ (millis >>> 32));
    }

    /** "Thu Jan 01 00:00:00 GMT 1970", in the default time zone. */
    public String toString() {
        Calendar c = Calendar.getInstance();
        c.setTime(this);
        StringBuffer sb = new StringBuffer(28);
        sb.append(DAYS[c.get(Calendar.DAY_OF_WEEK) - 1]).append(' ');
        sb.append(MONTHS[c.get(Calendar.MONTH)]).append(' ');
        twoDigits(sb, c.get(Calendar.DAY_OF_MONTH)).append(' ');
        twoDigits(sb, c.get(Calendar.HOUR_OF_DAY)).append(':');
        twoDigits(sb, c.get(Calendar.MINUTE)).append(':');
        twoDigits(sb, c.get(Calendar.SECOND)).append(' ');
        sb.append(c.getTimeZone().getID()).append(' ');
        return sb.append(c.get(Calendar.YEAR)).toString();
    }

    private static StringBuffer twoDigits(StringBuffer sb, int v) {
        if (v < 10) {
            sb.append('0');
        }
        return sb.append(v);
    }
}
