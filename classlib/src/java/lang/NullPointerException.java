package java.lang;

/** null was used where an object is required. */
public class NullPointerException extends RuntimeException {
    public NullPointerException() {
    }

    public NullPointerException(String message) {
        super(message);
    }
}
