package javax.microedition.rms;

/** No record store has that name. */
public class RecordStoreNotFoundException extends RecordStoreException {
    public RecordStoreNotFoundException() {
    }

    public RecordStoreNotFoundException(String message) {
        super(message);
    }
}
