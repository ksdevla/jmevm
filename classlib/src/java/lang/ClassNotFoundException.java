package java.lang;

/** A class named by a string was not found. */
public class ClassNotFoundException extends Exception {
    public ClassNotFoundException() {
    }

    public ClassNotFoundException(String message) {
        super(message);
    }
}
