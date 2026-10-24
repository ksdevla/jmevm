package java.io;

/** The end of a stream was reached unexpectedly. */
public class EOFException extends IOException {
    public EOFException() {
    }

    public EOFException(String message) {
        super(message);
    }
}
