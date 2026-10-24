package java.io;

/** The character encoding is not supported. */
public class UnsupportedEncodingException extends IOException {
    public UnsupportedEncodingException() {
    }

    public UnsupportedEncodingException(String message) {
        super(message);
    }
}
