package java.lang;

/** A class file failed verification. */
public class VerifyError extends LinkageError {
    public VerifyError() {
    }

    public VerifyError(String message) {
        super(message);
    }
}
