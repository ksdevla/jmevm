package javax.microedition.rms;

/** A two-way cursor over some of the records of a store, in some order. */
public interface RecordEnumeration {
    int numRecords();

    byte[] nextRecord() throws InvalidRecordIDException, RecordStoreNotOpenException, RecordStoreException;

    int nextRecordId() throws InvalidRecordIDException;

    byte[] previousRecord() throws InvalidRecordIDException, RecordStoreNotOpenException, RecordStoreException;

    int previousRecordId() throws InvalidRecordIDException;

    boolean hasNextElement();

    boolean hasPreviousElement();

    void reset();

    void rebuild();

    void keepUpdated(boolean keepUpdated);

    boolean isKeptUpdated();

    void destroy();
}
