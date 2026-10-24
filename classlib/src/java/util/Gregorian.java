package java.util;

/**
 * The Gregorian calendar, extended back before 1582 (proleptic). Dates
 * convert through a count of days since 1970-01-01, with the 400-year
 * cycle of 146097 days split into a year and a day of a March-based year,
 * which puts the leap day last.
 */
final class Gregorian extends Calendar {
    private static final int DAY = 24 * 60 * 60 * 1000;
    /* Days from 0000-03-01 to 1970-01-01. */
    private static final long EPOCH_DAYS = 719468;

    Gregorian() {
    }

    protected void computeFields() {
        TimeZone zone = getTimeZone();
        int offset = zone.getRawOffset();
        setLocal(time + offset);
        if (zone.useDaylightTime()) {
            int dst = zone.getOffset(1, fields[YEAR], fields[MONTH], fields[DATE], fields[DAY_OF_WEEK],
                    millisOfDay());
            if (dst != offset) {
                setLocal(time + dst);
            }
        }
    }

    private int millisOfDay() {
        return ((fields[HOUR_OF_DAY] * 60 + fields[MINUTE]) * 60 + fields[SECOND]) * 1000 + fields[MILLISECOND];
    }

    /** Sets the fields of a local time in ms since 1970-01-01 00:00. */
    private void setLocal(long local) {
        long days = floorDiv(local, DAY);
        int ms = (int) (local - days * DAY);
        long[] date = dateOf(days);
        fields[YEAR] = (int) date[0];
        fields[MONTH] = (int) date[1];
        fields[DATE] = (int) date[2];
        fields[DAY_OF_WEEK] = dayOfWeek(days);
        int hour = ms / 3600000;
        fields[HOUR_OF_DAY] = hour;
        fields[AM_PM] = hour < 12 ? AM : PM;
        fields[HOUR] = hour % 12;
        fields[MINUTE] = ms / 60000 % 60;
        fields[SECOND] = ms / 1000 % 60;
        fields[MILLISECOND] = ms % 1000;
    }

    /** {year, month (0 = January), day of month} of a day counted from 1970-01-01. */
    private static long[] dateOf(long days) {
        long z = days + EPOCH_DAYS;
        long era = floorDiv(z, 146097);
        int dayOfEra = (int) (z - era * 146097);                  /* 0 to 146096 */
        int yearOfEra = (dayOfEra - dayOfEra / 1460 + dayOfEra / 36524 - dayOfEra / 146096) / 365;
        int dayOfYear = dayOfEra - (365 * yearOfEra + yearOfEra / 4 - yearOfEra / 100);   /* from March 1st */
        int monthFromMarch = (5 * dayOfYear + 2) / 153;
        int day = dayOfYear - (153 * monthFromMarch + 2) / 5 + 1;
        int month = monthFromMarch < 10 ? monthFromMarch + 2 : monthFromMarch - 10;
        long year = era * 400 + yearOfEra + (month <= FEBRUARY ? 1 : 0);
        return new long[] {year, month, day};
    }

    private static int dayOfWeek(long days) {
        return (int) floorMod(days + 4, 7) + SUNDAY;   /* 1970-01-01 was a Thursday */
    }

    protected void computeTime() {
        long year = fields[YEAR] + floorDiv(fields[MONTH], 12);
        int month = (int) floorMod(fields[MONTH], 12);
        long days = daysOf(year, month) + fields[DATE] - 1;
        int hour = setAfter(HOUR_OF_DAY, HOUR) && setAfter(HOUR_OF_DAY, AM_PM)
                ? fields[HOUR_OF_DAY] : fields[AM_PM] * 12 + fields[HOUR];
        long local = days * DAY + ((hour * 60L + fields[MINUTE]) * 60 + fields[SECOND]) * 1000 + fields[MILLISECOND];

        TimeZone zone = getTimeZone();
        int offset = zone.getRawOffset();
        if (zone.useDaylightTime()) {
            long d = floorDiv(local, DAY);
            long[] date = dateOf(d);
            offset = zone.getOffset(1, (int) date[0], (int) date[1], (int) date[2], dayOfWeek(d),
                    (int) (local - d * DAY));
        }
        time = local - offset;
    }

    /** Days from 1970-01-01 to the first of a month (0 = January) of a year. */
    private static long daysOf(long year, int month) {
        long y = month <= FEBRUARY ? year - 1 : year;   /* years start in March */
        long era = floorDiv(y, 400);
        int yearOfEra = (int) (y - era * 400);
        int monthFromMarch = month >= MARCH ? month - 2 : month + 10;
        int dayOfYear = (153 * monthFromMarch + 2) / 5;
        int dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
        return era * 146097 + dayOfEra - EPOCH_DAYS;
    }

    private static long floorDiv(long a, long b) {
        long q = a / b;
        return (a % b != 0 && (a < 0) != (b < 0)) ? q - 1 : q;
    }

    private static long floorMod(long a, long b) {
        return a - floorDiv(a, b) * b;
    }
}
