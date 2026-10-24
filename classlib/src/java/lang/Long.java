package java.lang;

/** A long as an object, and conversions between longs and strings. */
public final class Long {
    public static final long MIN_VALUE = 0x8000000000000000L;
    public static final long MAX_VALUE = 0x7fffffffffffffffL;

    private final long value;

    public Long(long value) {
        this.value = value;
    }

    public static String toString(long i) {
        return toString(i, 10);
    }

    /** i in a radix from 2 to 36 (others mean 10), with a '-' if negative. */
    public static String toString(long i, int radix) {
        if (radix < Character.MIN_RADIX || radix > Character.MAX_RADIX) {
            radix = 10;
        }
        char[] buf = new char[radix < 8 ? 65 : 23];   /* digits and a sign */
        int pos = buf.length;
        boolean negative = i < 0;
        /* Work with the negative value: it has room for MIN_VALUE. */
        long n = negative ? i : -i;
        do {
            buf[--pos] = Integer.DIGITS[(int) -(n % radix)];
            n /= radix;
        } while (n != 0);
        if (negative) {
            buf[--pos] = '-';
        }
        return new String(buf, pos, buf.length - pos);
    }

    /** The bits of i as an unsigned number, shift bits a digit (hex: 4). */
    static String toUnsignedString(long i, int shift) {
        char[] buf = new char[64];
        int pos = buf.length;
        long mask = (1 << shift) - 1;
        do {
            buf[--pos] = Integer.DIGITS[(int) (i & mask)];
            i >>>= shift;
        } while (i != 0);
        return new String(buf, pos, buf.length - pos);
    }

    public static long parseLong(String s) throws NumberFormatException {
        return parseLong(s, 10);
    }

    /** A signed long in a radix: an optional '-' or '+', then digits. */
    public static long parseLong(String s, int radix) throws NumberFormatException {
        return parse(s, radix, MIN_VALUE, MAX_VALUE);
    }

    /**
     * Parses a signed number that must lie between min (negative) and max.
     * The digits accumulate as a negative number, which has the larger range.
     */
    static long parse(String s, int radix, long min, long max) {
        if (s == null) {
            throw new NumberFormatException("null");
        }
        if (radix < Character.MIN_RADIX) {
            throw new NumberFormatException("radix " + radix + " less than Character.MIN_RADIX");
        }
        if (radix > Character.MAX_RADIX) {
            throw new NumberFormatException("radix " + radix + " greater than Character.MAX_RADIX");
        }
        int n = s.length();
        int i = 0;
        boolean negative = false;
        if (n > 0 && (s.charAt(0) == '-' || s.charAt(0) == '+')) {
            negative = s.charAt(0) == '-';
            i = 1;
        }
        if (i == n) {
            throw badNumber(s, radix);
        }
        long limit = negative ? min : -max;
        long lastSafe = limit / radix;   /* below this, one more digit overflows */
        long result = 0;
        for (; i < n; i++) {
            int digit = Character.digit(s.charAt(i), radix);
            if (digit < 0 || result < lastSafe) {
                throw badNumber(s, radix);
            }
            result *= radix;
            if (result < limit + digit) {
                throw badNumber(s, radix);
            }
            result -= digit;
        }
        return negative ? result : -result;
    }

    private static NumberFormatException badNumber(String s, int radix) {
        return new NumberFormatException("For input string: \"" + s + '"' + (radix == 10 ? "" : " under radix " + radix));
    }

    public long longValue() {
        return value;
    }

    public float floatValue() {
        return value;
    }

    public double doubleValue() {
        return value;
    }

    public String toString() {
        return toString(value, 10);
    }

    public int hashCode() {
        return (int) (value ^ (value >>> 32));
    }

    public boolean equals(Object obj) {
        return obj instanceof Long && ((Long) obj).value == value;
    }
}
