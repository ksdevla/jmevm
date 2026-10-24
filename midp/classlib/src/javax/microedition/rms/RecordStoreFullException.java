package javax.microedition.rms;

/** There is no room left. */
public class RecordStoreFullException extends RecordStoreException {
    public RecordStoreFullException() {
    }

    public RecordStoreFullException(String message) {
        super(message);
    }
}
