package java.lang;

/** A thread used a monitor it does not own. */
public class IllegalMonitorStateException extends RuntimeException {
    public IllegalMonitorStateException() {
    }

    public IllegalMonitorStateException(String message) {
        super(message);
    }
}
