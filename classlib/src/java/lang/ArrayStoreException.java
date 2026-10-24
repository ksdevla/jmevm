package java.lang;

/** An object of the wrong type was stored in an array. */
public class ArrayStoreException extends RuntimeException {
    public ArrayStoreException() {
    }

    public ArrayStoreException(String message) {
        super(message);
    }
}
