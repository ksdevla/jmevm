package java.lang;

/** A method was passed an illegal argument. */
public class IllegalArgumentException extends RuntimeException {
    public IllegalArgumentException() {
    }

    public IllegalArgumentException(String message) {
        super(message);
    }
}
