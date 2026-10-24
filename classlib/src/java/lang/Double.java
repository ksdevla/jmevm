package java.lang;

/**
 * A double as an object. Conversions to and from strings and bits are the
 * virtual machine's natives.
 */
public final class Double {
    public static final double POSITIVE_INFINITY = 1.0 / 0.0;
    public static final double NEGATIVE_INFINITY = -1.0 / 0.0;
    public static final double NaN = 0.0 / 0.0;
    public static final double MAX_VALUE = 1.7976931348623157E308;
    public static final double MIN_VALUE = 4.9E-324;

    private final double value;

    public Double(double value) {
        this.value = value;
    }

    /** The shortest decimal that reads back as d, as Java prints it. */
    public static native String toString(double d);

    /** A decimal number, with leading and trailing white space allowed. */
    public static native double parseDouble(String s) throws NumberFormatException;

    public static Double valueOf(String s) throws NumberFormatException {
        return new Double(parseDouble(s));
    }

    public static boolean isNaN(double v) {
        return v != v;
    }

    public static boolean isInfinite(double v) {
        return v == POSITIVE_INFINITY || v == NEGATIVE_INFINITY;
    }

    /** The IEEE 754 bits of value, with every NaN made the canonical one. */
    public static native long doubleToLongBits(double value);

    public static native double longBitsToDouble(long bits);

    public boolean isNaN() {
        return isNaN(value);
    }

    public boolean isInfinite() {
        return isInfinite(value);
    }

    public byte byteValue() {
        return (byte) value;
    }

    public short shortValue() {
        return (short) value;
    }

    public int intValue() {
        return (int) value;
    }

    public long longValue() {
        return (long) value;
    }

    public float floatValue() {
        return (float) value;
    }

    public double doubleValue() {
        return value;
    }

    public String toString() {
        return toString(value);
    }

    public int hashCode() {
        long bits = doubleToLongBits(value);
        return (int) (bits ^ (bits >>> 32));
    }

    /** Equal bits: NaN equals NaN, and +0.0 does not equal -0.0. */
    public boolean equals(Object obj) {
        return obj instanceof Double && doubleToLongBits(((Double) obj).value) == doubleToLongBits(value);
    }
}
