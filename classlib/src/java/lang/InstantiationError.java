package java.lang;

/** new was used on an interface or abstract class. */
public class InstantiationError extends IncompatibleClassChangeError {
    public InstantiationError() {
    }

    public InstantiationError(String message) {
        super(message);
    }
}
