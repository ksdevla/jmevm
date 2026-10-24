package java.util;

/**
 * Converts between a moment (ms since 1970 UTC) and the calendar fields of
 * a time zone. getInstance() gives the Gregorian calendar.
 *
 * <p>Setting a field leaves the moment to be recomputed when it is next
 * needed; fields set out of range carry over (day 32 of January is
 * February 1st). Of HOUR_OF_DAY and HOUR with AM_PM, the one set last wins.
 */
public abstract class Calendar {
    public static final int YEAR = 1;
    public static final int MONTH = 2;
    public static final int DATE = 5;
    public static final int DAY_OF_MONTH = 5;
    public static final int DAY_OF_WEEK = 7;
    public static final int AM_PM = 9;
    public static final int HOUR = 10;
    public static final int HOUR_OF_DAY = 11;
    public static final int MINUTE = 12;
    public static final int SECOND = 13;
    public static final int MILLISECOND = 14;

    public static final int SUNDAY = 1;
    public static final int MONDAY = 2;
    public static final int TUESDAY = 3;
    public static final int WEDNESDAY = 4;
    public static final int THURSDAY = 5;
    public static final int FRIDAY = 6;
    public static final int SATURDAY = 7;

    public static final int JANUARY = 0;
    public static final int FEBRUARY = 1;
    public static final int MARCH = 2;
    public static final int APRIL = 3;
    public static final int MAY = 4;
    public static final int JUNE = 5;
    public static final int JULY = 6;
    public static final int AUGUST = 7;
    public static final int SEPTEMBER = 8;
    public static final int OCTOBER = 9;
    public static final int NOVEMBER = 10;
    public static final int DECEMBER = 11;

    public static final int AM = 0;
    public static final int PM = 1;

    static final int FIELD_COUNT = 15;

    /** The calendar fields, by the field numbers above. */
    protected int[] fields = new int[FIELD_COUNT];
    /** Which fields were set. */
    protected boolean[] isSet = new boolean[FIELD_COUNT];
    /** The moment, in ms since 1970-01-01 00:00 UTC. */
    protected long time;

    private boolean timeValid;     /* time agrees with the fields */
    private boolean fieldsValid;   /* the fields agree with time */
    private final int[] setOrder = new int[FIELD_COUNT];   /* when each field was set */
    private int setCount;
    private TimeZone zone;

    protected Calendar() {
        zone = TimeZone.getDefault();
    }

    public static synchronized Calendar getInstance() {
        return getInstance(TimeZone.getDefault());
    }

    /** A Gregorian calendar of a zone, set to now. */
    public static synchronized Calendar getInstance(TimeZone zone) {
        Calendar c = new Gregorian();
        c.setTimeZone(zone);
        c.setTimeInMillis(System.currentTimeMillis());
        return c;
    }

    public final Date getTime() {
        return new Date(getTimeInMillis());
    }

    public final void setTime(Date date) {
        setTimeInMillis(date.getTime());
    }

    protected long getTimeInMillis() {
        if (!timeValid) {
            computeTime();
            timeValid = true;
            fieldsValid = false;   /* normalize the fields when next read */
        }
        return time;
    }

    protected void setTimeInMillis(long millis) {
        time = millis;
        timeValid = true;
        computeFields();
        fieldsValid = true;
        for (int i = 0; i < FIELD_COUNT; i++) {
            isSet[i] = true;
        }
    }

    /** Brings the fields up to date: recomputes time if fields were set, then the fields from it. */
    private void complete() {
        long t = getTimeInMillis();
        if (!fieldsValid) {
            setTimeInMillis(t);
        }
    }

    public final int get(int field) {
        complete();
        return fields[field];
    }

    /* Fields set one after another are not normalized in between: setting
     * DATE to 31, then MONTH to FEBRUARY, gives February 31st, which reads
     * back as early March. */
    public final void set(int field, int value) {
        if (!fieldsValid) {
            complete();
        }
        fields[field] = value;
        isSet[field] = true;
        setOrder[field] = ++setCount;
        timeValid = false;
    }

    /** Whether field a was set more recently than field b. */
    final boolean setAfter(int a, int b) {
        return setOrder[a] > setOrder[b];
    }

    /** The same moment (calendars of any kind compare by their moments). */
    public boolean equals(Object obj) {
        return obj instanceof Calendar && ((Calendar) obj).getTimeInMillis() == getTimeInMillis();
    }

    public int hashCode() {
        long t = getTimeInMillis();
        return (int) (t ^ (t >>> 32));
    }

    public boolean before(Object when) {
        return when instanceof Calendar && getTimeInMillis() < ((Calendar) when).getTimeInMillis();
    }

    public boolean after(Object when) {
        return when instanceof Calendar && getTimeInMillis() > ((Calendar) when).getTimeInMillis();
    }

    /** Changes the zone, keeping the moment; the fields follow the new zone. */
    public void setTimeZone(TimeZone value) {
        long t = getTimeInMillis();
        zone = value;
        setTimeInMillis(t);
    }

    public TimeZone getTimeZone() {
        return zone;
    }

    /** Sets fields from time. */
    protected abstract void computeFields();

    /** Sets time from fields. */
    protected abstract void computeTime();
}
