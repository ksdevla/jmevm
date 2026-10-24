package javax.microedition.rms;

/** The record store is closed. */
public class RecordStoreNotOpenException extends RecordStoreException {
    public RecordStoreNotOpenException() {
    }

    public RecordStoreNotOpenException(String message) {
        super(message);
    }
}
