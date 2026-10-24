package java.lang;

/** A class file is malformed. */
public class ClassFormatError extends LinkageError {
    public ClassFormatError() {
    }

    public ClassFormatError(String message) {
        super(message);
    }
}
