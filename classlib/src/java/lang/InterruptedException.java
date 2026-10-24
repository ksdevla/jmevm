package java.lang;

/** A waiting or sleeping thread was interrupted. */
public class InterruptedException extends Exception {
    public InterruptedException() {
    }

    public InterruptedException(String message) {
        super(message);
    }
}
