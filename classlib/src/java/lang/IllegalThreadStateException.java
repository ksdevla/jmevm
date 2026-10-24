package java.lang;

/** A thread is not in the state the operation needs. */
public class IllegalThreadStateException extends IllegalArgumentException {
    public IllegalThreadStateException() {
    }

    public IllegalThreadStateException(String message) {
        super(message);
    }
}
