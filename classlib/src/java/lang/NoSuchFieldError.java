package java.lang;

/** A field no longer exists. */
public class NoSuchFieldError extends IncompatibleClassChangeError {
    public NoSuchFieldError() {
    }

    public NoSuchFieldError(String message) {
        super(message);
    }
}
