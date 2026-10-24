package java.lang;

/** Exceptions a method need not declare. */
public class RuntimeException extends Exception {
    public RuntimeException() {
    }

    public RuntimeException(String message) {
        super(message);
    }
}
