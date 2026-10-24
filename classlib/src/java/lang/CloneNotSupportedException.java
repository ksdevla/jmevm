package java.lang;

/** The object cannot be cloned. */
public class CloneNotSupportedException extends Exception {
    public CloneNotSupportedException() {
    }

    public CloneNotSupportedException(String message) {
        super(message);
    }
}
