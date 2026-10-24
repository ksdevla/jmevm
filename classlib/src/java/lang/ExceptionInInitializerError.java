package java.lang;

/** A static initializer threw an exception. */
public class ExceptionInInitializerError extends LinkageError {
    public ExceptionInInitializerError() {
    }

    public ExceptionInInitializerError(String message) {
        super(message);
    }
}
