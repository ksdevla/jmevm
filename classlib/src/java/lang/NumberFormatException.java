package java.lang;

/** A string does not hold a number in the expected format. */
public class NumberFormatException extends IllegalArgumentException {
    public NumberFormatException() {
    }

    public NumberFormatException(String message) {
        super(message);
    }
}
