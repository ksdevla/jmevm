package java.lang;

/** A method no longer exists. */
public class NoSuchMethodError extends IncompatibleClassChangeError {
    public NoSuchMethodError() {
    }

    public NoSuchMethodError(String message) {
        super(message);
    }
}
