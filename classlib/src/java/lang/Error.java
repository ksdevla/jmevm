package java.lang;

/** Serious problems a program should not try to catch. */
public class Error extends Throwable {
    public Error() {
    }

    public Error(String message) {
        super(message);
    }
}
