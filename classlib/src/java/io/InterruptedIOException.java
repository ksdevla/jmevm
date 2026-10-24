package java.io;

/** An input or output operation was interrupted. */
public class InterruptedIOException extends IOException {
    /** How many bytes had been transferred when the operation stopped. */
    public int bytesTransferred;

    public InterruptedIOException() {
    }

    public InterruptedIOException(String message) {
        super(message);
    }
}
