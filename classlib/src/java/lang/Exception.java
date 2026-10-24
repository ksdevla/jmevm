package java.lang;

/** Conditions a program might want to catch. */
public class Exception extends Throwable {
    public Exception() {
    }

    public Exception(String message) {
        super(message);
    }
}
