package java.lang;

/** A security violation. */
public class SecurityException extends RuntimeException {
    public SecurityException() {
    }

    public SecurityException(String message) {
        super(message);
    }
}
