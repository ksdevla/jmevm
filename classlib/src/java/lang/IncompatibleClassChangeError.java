package java.lang;

/** A class changed incompatibly since it was compiled against. */
public class IncompatibleClassChangeError extends LinkageError {
    public IncompatibleClassChangeError() {
    }

    public IncompatibleClassChangeError(String message) {
        super(message);
    }
}
