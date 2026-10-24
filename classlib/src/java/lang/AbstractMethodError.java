package java.lang;

/** An abstract method was called. */
public class AbstractMethodError extends IncompatibleClassChangeError {
    public AbstractMethodError() {
    }

    public AbstractMethodError(String message) {
        super(message);
    }
}
