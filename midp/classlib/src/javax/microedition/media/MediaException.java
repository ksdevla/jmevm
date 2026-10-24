package javax.microedition.media;

/** A media operation could not be done. */
public class MediaException extends Exception {
    public MediaException() {
    }

    public MediaException(String reason) {
        super(reason);
    }
}
