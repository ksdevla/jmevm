package java.lang;

/** A class depends on another that changed incompatibly. */
public class LinkageError extends Error {
    public LinkageError() {
    }

    public LinkageError(String message) {
        super(message);
    }
}
