package java.lang;

/** A class or its constructor is not accessible. */
public class IllegalAccessException extends Exception {
    public IllegalAccessException() {
    }

    public IllegalAccessException(String message) {
        super(message);
    }
}
