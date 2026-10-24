package java.lang;

/** An object was cast to a class it is not an instance of. */
public class ClassCastException extends RuntimeException {
    public ClassCastException() {
    }

    public ClassCastException(String message) {
        super(message);
    }
}
