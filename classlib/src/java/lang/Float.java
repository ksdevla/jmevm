package java.lang;

/**
 * A float as an object. Conversions to and from strings and bits are the
 * virtual machine's natives.
 */
public final class Float {
    public static final float POSITIVE_INFINITY = 1.0f / 0.0f;
    public static final float NEGATIVE_INFINITY = -1.0f / 0.0f;
    public static final float NaN = 0.0f / 0.0f;
    public static final float MAX_VALUE = 3.4028235E38f;
    public static final float MIN_VALUE = 1.4E-45f;

    private final float value;

    public Float(float value) {
        this.value = value;
    }

    public Float(double value) {
        this.value = (float) value;
    }

    /** The shortest decimal that reads back as f, as Java prints it. */
    public static native String toString(float f);

    /** A decimal number, with leading and trailing white space allowed. */
    public static native float parseFloat(String s) throws NumberFormatException;

    public static Float valueOf(String s) throws NumberFormatException {
        return new Float(parseFloat(s));
    }

    public static boolean isNaN(float v) {
        return v != v;
    }

    public static boolean isInfinite(float v) {
        return v == POSITIVE_INFINITY || v == NEGATIVE_INFINITY;
    }

    /** The IEEE 754 bits of value, with every NaN made the canonical one. */
    public static native int floatToIntBits(float value);

    public static native float intBitsToFloat(int bits);

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
        return value;
    }

    public double doubleValue() {
        return value;
    }

    public String toString() {
        return toString(value);
    }

    public int hashCode() {
        return floatToIntBits(value);
    }

    /** Equal bits: NaN equals NaN, and +0.0 does not equal -0.0. */
    public boolean equals(Object obj) {
        return obj instanceof Float && floatToIntBits(((Float) obj).value) == floatToIntBits(value);
    }
}
