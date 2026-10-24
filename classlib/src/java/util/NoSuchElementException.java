package java.util;

/** An enumeration has no more elements. */
public class NoSuchElementException extends RuntimeException {
    public NoSuchElementException() {
    }

    public NoSuchElementException(String message) {
        super(message);
    }
}
