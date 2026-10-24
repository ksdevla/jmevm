package java.lang;

/** A class that was present when compiling cannot be found. */
public class NoClassDefFoundError extends LinkageError {
    public NoClassDefFoundError() {
    }

    public NoClassDefFoundError(String message) {
        super(message);
    }
}
