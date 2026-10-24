package javax.microedition.rms;

/** Hears about the changes of a record store, after they are made. */
public interface RecordListener {
    void recordAdded(RecordStore recordStore, int recordId);

    void recordChanged(RecordStore recordStore, int recordId);

    void recordDeleted(RecordStore recordStore, int recordId);
}
