package java.lang;

/** An int as an object, and conversions between ints and strings. */
public final class Integer {
    public static final int MIN_VALUE = 0x80000000;
    public static final int MAX_VALUE = 0x7fffffff;

    static final char[] DIGITS = "0123456789abcdefghijklmnopqrstuvwxyz".toCharArray();

    private final int value;

    public Integer(int value) {
        this.value = value;
    }

    public static String toString(int i) {
        return Long.toString(i, 10);
    }

    /** i in a radix from 2 to 36 (others mean 10), with a '-' if negative. */
    public static String toString(int i, int radix) {
        return Long.toString(i, radix);
    }

    public static String toHexString(int i) {
        return Long.toUnsignedString(i & 0xffffffffL, 4);
    }

    public static String toOctalString(int i) {
        return Long.toUnsignedString(i & 0xffffffffL, 3);
    }

    public static String toBinaryString(int i) {
        return Long.toUnsignedString(i & 0xffffffffL, 1);
    }

    public static int parseInt(String s) throws NumberFormatException {
        return parseInt(s, 10);
    }

    /** A signed int in a radix: an optional '-' or '+', then digits. */
    public static int parseInt(String s, int radix) throws NumberFormatException {
        return (int) Long.parse(s, radix, MIN_VALUE, MAX_VALUE);
    }

    /** parseInt, but the value must lie between min and max (Byte, Short). */
    static int parseInRange(String s, int radix, int min, int max) {
        int v = parseInt(s, radix);
        if (v < min || v > max) {
            throw new NumberFormatException("Value out of range. Value:\"" + s + "\" Radix:" + radix);
        }
        return v;
    }

    public static Integer valueOf(String s) throws NumberFormatException {
        return new Integer(parseInt(s, 10));
    }

    public static Integer valueOf(String s, int radix) throws NumberFormatException {
        return new Integer(parseInt(s, radix));
    }

    public byte byteValue() {
        return (byte) value;
    }

    public short shortValue() {
        return (short) value;
    }

    public int intValue() {
        return value;
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
        return toString(value);
    }

    public int hashCode() {
        return value;
    }

    public boolean equals(Object obj) {
        return obj instanceof Integer && ((Integer) obj).value == value;
    }
}
