package java.io;

/** An input or output operation failed or was interrupted. */
public class IOException extends Exception {
    public IOException() {
    }

    public IOException(String message) {
        super(message);
    }
}
