package java.lang;

/** A string was indexed below zero or at or beyond its length. */
public class StringIndexOutOfBoundsException extends IndexOutOfBoundsException {
    public StringIndexOutOfBoundsException() {
    }

    public StringIndexOutOfBoundsException(int index) {
        super("String index out of range: " + index);
    }

    public StringIndexOutOfBoundsException(String message) {
        super(message);
    }
}
