package java.lang;

/** An array was created with a negative size. */
public class NegativeArraySizeException extends RuntimeException {
    public NegativeArraySizeException() {
    }

    public NegativeArraySizeException(String message) {
        super(message);
    }
}
