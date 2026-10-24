package java.lang;

/** An array was indexed below zero or at or beyond its length. */
public class ArrayIndexOutOfBoundsException extends IndexOutOfBoundsException {
    public ArrayIndexOutOfBoundsException() {
    }

    public ArrayIndexOutOfBoundsException(int index) {
        super("Array index out of range: " + index);
    }

    public ArrayIndexOutOfBoundsException(String message) {
        super(message);
    }
}
