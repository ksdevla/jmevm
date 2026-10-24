package java.lang;

/** An index is out of range. */
public class IndexOutOfBoundsException extends RuntimeException {
    public IndexOutOfBoundsException() {
    }

    public IndexOutOfBoundsException(String message) {
        super(message);
    }
}
