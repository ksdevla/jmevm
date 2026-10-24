package java.lang;

/** A method was called at a time it is not allowed. */
public class IllegalStateException extends RuntimeException {
    public IllegalStateException() {
    }

    public IllegalStateException(String message) {
        super(message);
    }
}
