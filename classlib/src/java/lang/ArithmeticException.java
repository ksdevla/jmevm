package java.lang;

/** An arithmetic error, such as an integer division by zero. */
public class ArithmeticException extends RuntimeException {
    public ArithmeticException() {
    }

    public ArithmeticException(String message) {
        super(message);
    }
}
