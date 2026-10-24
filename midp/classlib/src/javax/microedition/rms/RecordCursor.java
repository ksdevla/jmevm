package javax.microedition.rms;

/**
 * The RecordEnumeration of a store: the ids of the records the filter
 * accepts, in the comparator's order (stable) or in ascending id order.
 *
 * <p>at is the position of the record returned last, or -1 before the first:
 * from there the next record is the first one and the previous record the
 * last one. Kept updated, the cursor listens to the store and rebuilds its
 * list after every change, staying at the same record where it can.
 */
final class RecordCursor implements RecordEnumeration, RecordListener {
    private final RecordStore store;
    private final RecordFilter filter;
    private final RecordComparator comparator;
    private boolean live;
    private boolean destroyed;
    private int[] ids = new int[0];
    private int at = -1;

    RecordCursor(RecordStore store, RecordFilter filter, RecordComparator comparator, boolean keepUpdated) {
        this.store = store;
        this.filter = filter;
        this.comparator = comparator;
        build();
        keepUpdated(keepUpdated);
    }

    private void check() {
        if (destroyed) {
            throw new IllegalStateException("the enumeration was destroyed");
        }
    }

    private void build() {
        int[] all;
        try {
            all = store.recordIds();
        } catch (RecordStoreNotOpenException e) {
            all = new int[0];
        }
        int n = 0;
        int[] kept = new int[all.length];
        byte[][] data = new byte[all.length][];
        for (int i = 0; i < all.length; i++) {
            byte[] r = store.peek(all[i]);
            byte[] copy = r == null || r.length == 0 ? null : (byte[]) r.clone();
            if (r != null && (filter == null || filter.matches(copy))) {
                kept[n] = all[i];
                data[n] = copy;
                n++;
            }
        }
        if (comparator != null) {
            sort(kept, data, n);
        }
        int current = at >= 0 && at < ids.length ? ids[at] : -1;
        ids = new int[n];
        System.arraycopy(kept, 0, ids, 0, n);
        at = -1;
        for (int i = 0; current >= 0 && i < n; i++) {
            if (ids[i] == current) {
                at = i;
            }
        }
    }

    /** A stable insertion sort by the comparator: enumerations are short. */
    private void sort(int[] keys, byte[][] data, int n) {
        for (int i = 1; i < n; i++) {
            int k = keys[i];
            byte[] d = data[i];
            int j = i - 1;
            while (j >= 0 && comparator.compare(data[j], d) == RecordComparator.FOLLOWS) {
                keys[j + 1] = keys[j];
                data[j + 1] = data[j];
                j--;
            }
            keys[j + 1] = k;
            data[j + 1] = d;
        }
    }

    public synchronized int numRecords() {
        check();
        return ids.length;
    }

    public synchronized int nextRecordId() throws InvalidRecordIDException {
        check();
        int next = at + 1;
        if (next >= ids.length) {
            throw new InvalidRecordIDException("no next record");
        }
        at = next;
        return ids[at];
    }

    public synchronized int previousRecordId() throws InvalidRecordIDException {
        check();
        int previous = at < 0 ? ids.length - 1 : at - 1;
        if (previous < 0) {
            throw new InvalidRecordIDException("no previous record");
        }
        at = previous;
        return ids[at];
    }

    public byte[] nextRecord() throws InvalidRecordIDException, RecordStoreNotOpenException, RecordStoreException {
        if (!store.isOpen()) {
            throw new RecordStoreNotOpenException();
        }
        return store.getRecord(nextRecordId());
    }

    public byte[] previousRecord()
            throws InvalidRecordIDException, RecordStoreNotOpenException, RecordStoreException {
        if (!store.isOpen()) {
            throw new RecordStoreNotOpenException();
        }
        return store.getRecord(previousRecordId());
    }

    public synchronized boolean hasNextElement() {
        check();
        return at + 1 < ids.length;
    }

    public synchronized boolean hasPreviousElement() {
        check();
        return at < 0 ? ids.length > 0 : at > 0;
    }

    public synchronized void reset() {
        check();
        at = -1;
    }

    public synchronized void rebuild() {
        check();
        build();
    }

    public synchronized void keepUpdated(boolean keepUpdated) {
        check();
        if (keepUpdated && !live) {
            store.addRecordListener(this);
            build();
        } else if (!keepUpdated && live) {
            store.removeRecordListener(this);
        }
        live = keepUpdated;
    }

    public synchronized boolean isKeptUpdated() {
        check();
        return live;
    }

    public synchronized void destroy() {
        check();
        if (live) {
            store.removeRecordListener(this);
        }
        destroyed = true;
        ids = null;
    }

    public synchronized void recordAdded(RecordStore recordStore, int recordId) {
        build();
    }

    public synchronized void recordChanged(RecordStore recordStore, int recordId) {
        build();
    }

    public synchronized void recordDeleted(RecordStore recordStore, int recordId) {
        build();
    }
}
