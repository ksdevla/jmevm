package java.lang;

/** A class is its own superclass or superinterface. */
public class ClassCircularityError extends LinkageError {
    public ClassCircularityError() {
    }

    public ClassCircularityError(String message) {
        super(message);
    }
}
