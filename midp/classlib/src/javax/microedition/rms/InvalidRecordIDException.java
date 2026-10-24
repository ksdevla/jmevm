package javax.microedition.rms;

/** No record has that id. */
public class InvalidRecordIDException extends RecordStoreException {
    public InvalidRecordIDException() {
    }

    public InvalidRecordIDException(String message) {
        super(message);
    }
}
