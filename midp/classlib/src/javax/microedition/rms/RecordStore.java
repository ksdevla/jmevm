package javax.microedition.rms;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.util.Hashtable;
import java.util.Vector;

import jmevm.midp.AppProperties;
import jmevm.midp.Storage;

/**
 * A record store: numbered byte records that last between runs.
 *
 * <p>Each store is kept in memory and, after every change, written whole to
 * the platform's storage under "vendor/suite/name" (each part escaped), as:
 * "MVRS", u8 format (1), u8 auth mode, u8 writable, s32 version, s64 last
 * modified, s32 next id, s32 count, then per record s32 id, s32 length and
 * the bytes, in ascending id order. Big-endian.
 */
public class RecordStore {
    public static final int AUTHMODE_PRIVATE = 0;
    public static final int AUTHMODE_ANY = 1;

    private static final int FORMAT = 1;
    private static final int HEADER = 27;      /* bytes before the records */
    private static final int PER_RECORD = 8;
    private static final int QUOTA = 1 << 20;  /* bytes per store */
    private static final int MAX_NAME = 32;

    /** Open stores by storage key; also the lock of the static methods. */
    private static final Hashtable OPEN = new Hashtable();

    private final String name;
    private final String key;
    private final boolean own;
    private int opened;
    private int authmode;
    private boolean writable;
    private int version;
    private long modified;
    private int nextId = 1;
    private int count;
    private int[] ids = new int[8];
    private byte[][] records = new byte[8][];
    private int size = HEADER;
    private final Vector listeners = new Vector();

    private RecordStore(String name, String key, boolean own) {
        this.name = name;
        this.key = key;
        this.own = own;
    }

    // ---- names

    private static String escape(String s) {
        StringBuffer b = new StringBuffer(s.length() + 8);
        for (int i = 0; i < s.length(); i++) {
            char c = s.charAt(i);
            if (c == '%') {
                b.append("%25");
            } else if (c == '/') {
                b.append("%2F");
            } else {
                b.append(c);
            }
        }
        return b.toString();
    }

    private static String unescape(String s) {
        StringBuffer b = new StringBuffer(s.length());
        for (int i = 0; i < s.length(); i++) {
            if (s.startsWith("%25", i)) {
                b.append('%');
                i += 2;
            } else if (s.startsWith("%2F", i)) {
                b.append('/');
                i += 2;
            } else {
                b.append(s.charAt(i));
            }
        }
        return b.toString();
    }

    private static String property(String name, String fallback) {
        String v = AppProperties.get(name);
        return v != null && v.length() > 0 ? v : fallback;
    }

    private static String ownVendor() {
        return property("MIDlet-Vendor", "");
    }

    private static String ownSuite() {
        return property("MIDlet-Name", "default");
    }

    private static String prefix(String vendor, String suite) {
        return escape(vendor) + "/" + escape(suite) + "/";
    }

    private static void checkName(String name) {
        if (name == null) {
            throw new NullPointerException();
        }
        if (name.length() < 1 || name.length() > MAX_NAME) {
            throw new IllegalArgumentException("record store name of " + name.length() + " characters");
        }
    }

    // ---- opening and closing

    public static RecordStore openRecordStore(String recordStoreName, boolean createIfNecessary)
            throws RecordStoreException, RecordStoreFullException, RecordStoreNotFoundException {
        return open(recordStoreName, ownVendor(), ownSuite(), createIfNecessary, AUTHMODE_PRIVATE, false);
    }

    public static RecordStore openRecordStore(String recordStoreName, boolean createIfNecessary, int authmode,
            boolean writable) throws RecordStoreException, RecordStoreFullException, RecordStoreNotFoundException {
        checkMode(authmode);
        return open(recordStoreName, ownVendor(), ownSuite(), createIfNecessary, authmode, writable);
    }

    public static RecordStore openRecordStore(String recordStoreName, String vendorName, String suiteName)
            throws RecordStoreException, RecordStoreNotFoundException {
        if (vendorName == null || suiteName == null) {
            throw new IllegalArgumentException("vendor and suite are needed");
        }
        RecordStore rs = open(recordStoreName, vendorName, suiteName, false, AUTHMODE_PRIVATE, false);
        if (!rs.own && rs.authmode != AUTHMODE_ANY) {
            rs.closeRecordStore();
            throw new SecurityException("record store " + recordStoreName + " is not shared");
        }
        return rs;
    }

    private static void checkMode(int authmode) {
        if (authmode != AUTHMODE_PRIVATE && authmode != AUTHMODE_ANY) {
            throw new IllegalArgumentException("auth mode " + authmode);
        }
    }

    private static RecordStore open(String name, String vendor, String suite, boolean create, int authmode,
            boolean writable) throws RecordStoreException {
        checkName(name);
        String key = prefix(vendor, suite) + escape(name);
        synchronized (OPEN) {
            RecordStore rs = (RecordStore) OPEN.get(key);
            if (rs == null) {
                rs = new RecordStore(name, key, vendor.equals(ownVendor()) && suite.equals(ownSuite()));
                byte[] blob = Storage.read(key);
                if (blob != null) {
                    rs.load(blob);
                } else if (create) {
                    rs.authmode = authmode;
                    rs.writable = writable;
                    rs.modified = System.currentTimeMillis();
                    rs.save();
                } else {
                    throw new RecordStoreNotFoundException(name);
                }
                OPEN.put(key, rs);
            }
            rs.opened++;
            return rs;
        }
    }

    public void closeRecordStore() throws RecordStoreNotOpenException, RecordStoreException {
        synchronized (OPEN) {
            synchronized (this) {
                checkOpen();
                if (--opened == 0) {
                    OPEN.remove(key);
                    listeners.removeAllElements();
                }
            }
        }
    }

    public static void deleteRecordStore(String recordStoreName)
            throws RecordStoreException, RecordStoreNotFoundException {
        checkName(recordStoreName);
        String key = prefix(ownVendor(), ownSuite()) + escape(recordStoreName);
        synchronized (OPEN) {
            if (OPEN.get(key) != null) {
                throw new RecordStoreException("record store " + recordStoreName + " is open");
            }
            if (!Storage.delete(key)) {
                throw new RecordStoreNotFoundException(recordStoreName);
            }
        }
    }

    public static String[] listRecordStores() {
        String prefix = prefix(ownVendor(), ownSuite());
        String[] keys = Storage.list();
        Vector names = new Vector();
        for (int i = 0; keys != null && i < keys.length; i++) {
            String k = keys[i];
            if (k.startsWith(prefix) && k.indexOf('/', prefix.length()) < 0) {
                names.addElement(unescape(k.substring(prefix.length())));
            }
        }
        if (names.size() == 0) {
            return null;
        }
        String[] out = new String[names.size()];
        names.copyInto(out);
        return out;
    }

    // ---- the blob

    private void load(byte[] blob) throws RecordStoreException {
        try {
            DataInputStream in = new DataInputStream(new ByteArrayInputStream(blob));
            if (in.readByte() != 'M' || in.readByte() != 'V' || in.readByte() != 'R' || in.readByte() != 'S'
                    || in.readByte() != FORMAT) {
                throw new RecordStoreException("record store " + name + " is in an unknown format");
            }
            authmode = in.readByte();
            writable = in.readByte() != 0;
            version = in.readInt();
            modified = in.readLong();
            nextId = in.readInt();
            int n = in.readInt();
            if (n < 0 || n > blob.length / PER_RECORD) {
                throw new RecordStoreException("record store " + name + " is damaged");
            }
            ids = new int[Math.max(n, 8)];
            records = new byte[ids.length][];
            for (int i = 0; i < n; i++) {
                ids[i] = in.readInt();
                byte[] r = new byte[in.readInt()];
                in.readFully(r);
                records[i] = r;
            }
            count = n;
            size = blob.length;
        } catch (IOException e) {
            throw new RecordStoreException("record store " + name + " is damaged");
        }
    }

    private void save() throws RecordStoreException {
        ByteArrayOutputStream bytes = new ByteArrayOutputStream(size);
        DataOutputStream out = new DataOutputStream(bytes);
        try {
            out.write(new byte[] {'M', 'V', 'R', 'S', FORMAT});
            out.writeByte(authmode);
            out.writeByte(writable ? 1 : 0);
            out.writeInt(version);
            out.writeLong(modified);
            out.writeInt(nextId);
            out.writeInt(count);
            for (int i = 0; i < count; i++) {
                out.writeInt(ids[i]);
                out.writeInt(records[i].length);
                out.write(records[i]);
            }
        } catch (IOException e) {
            throw new RecordStoreException(e.toString());
        }
        byte[] blob = bytes.toByteArray();
        if (!Storage.write(key, blob)) {
            throw new RecordStoreException("cannot write record store " + name);
        }
        size = blob.length;
    }

    // ---- records

    private void checkOpen() throws RecordStoreNotOpenException {
        if (opened == 0) {
            throw new RecordStoreNotOpenException(name);
        }
    }

    private void checkWritable() {
        if (!own && !writable) {
            throw new SecurityException("record store " + name + " is read-only for other suites");
        }
    }

    /** The index of a record, or -(insertion point) - 1. */
    private int indexOf(int id) {
        int lo = 0, hi = count - 1;
        while (lo <= hi) {
            int mid = (lo + hi) >>> 1;
            if (ids[mid] < id) {
                lo = mid + 1;
            } else if (ids[mid] > id) {
                hi = mid - 1;
            } else {
                return mid;
            }
        }
        return -lo - 1;
    }

    private int existing(int id) throws InvalidRecordIDException {
        int i = indexOf(id);
        if (i < 0) {
            throw new InvalidRecordIDException("record " + id);
        }
        return i;
    }

    private static byte[] copy(byte[] data, int offset, int numBytes) {
        if (data == null && numBytes > 0) {
            throw new NullPointerException();
        }
        if (numBytes < 0 || offset < 0 || (data != null && offset > data.length - numBytes)) {
            throw new ArrayIndexOutOfBoundsException();
        }
        byte[] r = new byte[numBytes];
        if (numBytes > 0) {
            System.arraycopy(data, offset, r, 0, numBytes);
        }
        return r;
    }

    private void changed() {
        version++;
        modified = System.currentTimeMillis();
    }

    public int addRecord(byte[] data, int offset, int numBytes)
            throws RecordStoreNotOpenException, RecordStoreException, RecordStoreFullException {
        int id;
        synchronized (this) {
            checkOpen();
            checkWritable();
            byte[] r = copy(data, offset, numBytes);
            if (size + PER_RECORD + numBytes > QUOTA) {
                throw new RecordStoreFullException("record store " + name + " is full");
            }
            if (count == ids.length) {
                int[] moreIds = new int[count * 2];
                byte[][] moreRecords = new byte[count * 2][];
                System.arraycopy(ids, 0, moreIds, 0, count);
                System.arraycopy(records, 0, moreRecords, 0, count);
                ids = moreIds;
                records = moreRecords;
            }
            id = nextId++;
            ids[count] = id;   /* ids only grow: the new one goes last */
            records[count] = r;
            count++;
            changed();
            save();
        }
        tell(0, id);
        return id;
    }

    public void deleteRecord(int recordId)
            throws RecordStoreNotOpenException, InvalidRecordIDException, RecordStoreException {
        synchronized (this) {
            checkOpen();
            checkWritable();
            int i = existing(recordId);
            System.arraycopy(ids, i + 1, ids, i, count - i - 1);
            System.arraycopy(records, i + 1, records, i, count - i - 1);
            records[--count] = null;
            changed();
            save();
        }
        tell(2, recordId);
    }

    public void setRecord(int recordId, byte[] newData, int offset, int numBytes)
            throws RecordStoreNotOpenException, InvalidRecordIDException, RecordStoreException,
            RecordStoreFullException {
        synchronized (this) {
            checkOpen();
            checkWritable();
            int i = existing(recordId);
            byte[] r = copy(newData, offset, numBytes);
            if (size - records[i].length + numBytes > QUOTA) {
                throw new RecordStoreFullException("record store " + name + " is full");
            }
            records[i] = r;
            changed();
            save();
        }
        tell(1, recordId);
    }

    public synchronized byte[] getRecord(int recordId)
            throws RecordStoreNotOpenException, InvalidRecordIDException, RecordStoreException {
        checkOpen();
        byte[] r = records[existing(recordId)];
        return r.length == 0 ? null : copy(r, 0, r.length);
    }

    public synchronized int getRecord(int recordId, byte[] buffer, int offset)
            throws RecordStoreNotOpenException, InvalidRecordIDException, RecordStoreException {
        checkOpen();
        byte[] r = records[existing(recordId)];
        System.arraycopy(r, 0, buffer, offset, r.length);   /* throws if it does not fit */
        return r.length;
    }

    public synchronized int getRecordSize(int recordId)
            throws RecordStoreNotOpenException, InvalidRecordIDException, RecordStoreException {
        checkOpen();
        return records[existing(recordId)].length;
    }

    /** The record ids in ascending order, for enumerations. */
    synchronized int[] recordIds() throws RecordStoreNotOpenException {
        checkOpen();
        int[] out = new int[count];
        System.arraycopy(ids, 0, out, 0, count);
        return out;
    }

    /** A record as stored (not a copy), or null if it is gone. */
    synchronized byte[] peek(int recordId) {
        int i = indexOf(recordId);
        return i < 0 ? null : records[i];
    }

    // ---- listeners

    public void addRecordListener(RecordListener listener) {
        synchronized (listeners) {
            if (listener != null && !listeners.contains(listener)) {
                listeners.addElement(listener);
            }
        }
    }

    public void removeRecordListener(RecordListener listener) {
        listeners.removeElement(listener);
    }

    /** Tells the listeners: 0 added, 1 changed, 2 deleted. Not under the lock. */
    private void tell(int what, int id) {
        RecordListener[] ls;
        synchronized (listeners) {
            ls = new RecordListener[listeners.size()];
            listeners.copyInto(ls);
        }
        for (int i = 0; i < ls.length; i++) {
            if (what == 0) {
                ls[i].recordAdded(this, id);
            } else if (what == 1) {
                ls[i].recordChanged(this, id);
            } else {
                ls[i].recordDeleted(this, id);
            }
        }
    }

    // ---- the store as a whole

    public RecordEnumeration enumerateRecords(RecordFilter filter, RecordComparator comparator,
            boolean keepUpdated) throws RecordStoreNotOpenException {
        checkOpen();
        return new RecordCursor(this, filter, comparator, keepUpdated);
    }

    public synchronized String getName() throws RecordStoreNotOpenException {
        checkOpen();
        return name;
    }

    public synchronized int getVersion() throws RecordStoreNotOpenException {
        checkOpen();
        return version;
    }

    public synchronized int getNumRecords() throws RecordStoreNotOpenException {
        checkOpen();
        return count;
    }

    public synchronized int getSize() throws RecordStoreNotOpenException {
        checkOpen();
        return size;
    }

    public synchronized int getSizeAvailable() throws RecordStoreNotOpenException {
        checkOpen();
        return Math.max(0, QUOTA - size - PER_RECORD);
    }

    public synchronized long getLastModified() throws RecordStoreNotOpenException {
        checkOpen();
        return modified;
    }

    public synchronized int getNextRecordID() throws RecordStoreNotOpenException, RecordStoreException {
        checkOpen();
        return nextId;
    }

    public synchronized void setMode(int authmode, boolean writable) throws RecordStoreException {
        checkOpen();
        if (!own) {
            throw new SecurityException("only the owner can change record store " + name);
        }
        checkMode(authmode);
        this.authmode = authmode;
        this.writable = writable;
        save();
    }

    /** For enumerations: whether the store is still open. */
    synchronized boolean isOpen() {
        return opened > 0;
    }
}
