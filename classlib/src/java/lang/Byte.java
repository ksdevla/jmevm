package java.lang;

/** A byte as an object. */
public final class Byte {
    public static final byte MIN_VALUE = -128;
    public static final byte MAX_VALUE = 127;

    private final byte value;

    public Byte(byte value) {
        this.value = value;
    }

    public static byte parseByte(String s) throws NumberFormatException {
        return parseByte(s, 10);
    }

    public static byte parseByte(String s, int radix) throws NumberFormatException {
        return (byte) Integer.parseInRange(s, radix, MIN_VALUE, MAX_VALUE);
    }

    public byte byteValue() {
        return value;
    }

    public String toString() {
        return Integer.toString(value);
    }

    public int hashCode() {
        return value;
    }

    public boolean equals(Object obj) {
        return obj instanceof Byte && ((Byte) obj).value == value;
    }
}
