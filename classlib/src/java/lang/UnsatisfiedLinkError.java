package java.lang;

/** A native method has no implementation. */
public class UnsatisfiedLinkError extends LinkageError {
    public UnsatisfiedLinkError() {
    }

    public UnsatisfiedLinkError(String message) {
        super(message);
    }
}
