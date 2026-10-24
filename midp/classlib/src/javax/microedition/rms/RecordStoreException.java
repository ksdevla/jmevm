package javax.microedition.rms;

/** A general record store failure. */
public class RecordStoreException extends Exception {
    public RecordStoreException() {
    }

    public RecordStoreException(String message) {
        super(message);
    }
}
