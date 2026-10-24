package java.util;

/**
 * Maps keys to values, neither null, by the keys' hashCode() and equals(),
 * safe to share between threads.
 *
 * <p>The table is chained: a key's bucket is (hash & 0x7FFFFFFF) % capacity,
 * a new entry goes to the head of its bucket's chain, and a table more than
 * three quarters full grows to twice its capacity plus one. Enumerations go
 * from the last bucket to the first. That is the classic Hashtable layout,
 * so applications see their keys in the order they saw on phones.
 */
public class Hashtable {
    private Entry[] table;
    private int count;
    private int threshold;   /* grow when count reaches this */

    public Hashtable(int initialCapacity) {
        if (initialCapacity < 0) {
            throw new IllegalArgumentException("Illegal Capacity: " + initialCapacity);
        }
        if (initialCapacity == 0) {
            initialCapacity = 1;
        }
        table = new Entry[initialCapacity];
        threshold = initialCapacity * 3 / 4;
    }

    public Hashtable() {
        this(11);
    }

    public int size() {
        return count;
    }

    public boolean isEmpty() {
        return count == 0;
    }

    /** The keys, as they are when this is called. */
    public synchronized Enumeration keys() {
        return new Snapshot(collect(true));
    }

    /** The values, as they are when this is called. */
    public synchronized Enumeration elements() {
        return new Snapshot(collect(false));
    }

    private Object[] collect(boolean keys) {
        Object[] all = new Object[count];
        int n = 0;
        for (int i = table.length - 1; i >= 0; i--) {
            for (Entry e = table[i]; e != null; e = e.next) {
                all[n++] = keys ? e.key : e.value;
            }
        }
        return all;
    }

    /** Whether some key maps to value (compared with equals()). */
    public synchronized boolean contains(Object value) {
        if (value == null) {
            throw new NullPointerException();
        }
        for (int i = table.length - 1; i >= 0; i--) {
            for (Entry e = table[i]; e != null; e = e.next) {
                if (value.equals(e.value)) {
                    return true;
                }
            }
        }
        return false;
    }

    public synchronized boolean containsKey(Object key) {
        return find(key) != null;
    }

    public synchronized Object get(Object key) {
        Entry e = find(key);
        return e != null ? e.value : null;
    }

    private Entry find(Object key) {
        int hash = key.hashCode();
        for (Entry e = table[index(hash, table.length)]; e != null; e = e.next) {
            if (e.hash == hash && key.equals(e.key)) {
                return e;
            }
        }
        return null;
    }

    private static int index(int hash, int capacity) {
        return (hash & 0x7fffffff) % capacity;
    }

    /** Moves every entry to a table twice as big plus one. */
    protected void rehash() {
        Entry[] old = table;
        int capacity = old.length * 2 + 1;
        Entry[] grown = new Entry[capacity];
        for (int i = old.length - 1; i >= 0; i--) {
            Entry e = old[i];
            while (e != null) {
                Entry next = e.next;
                int j = index(e.hash, capacity);
                e.next = grown[j];
                grown[j] = e;
                e = next;
            }
        }
        table = grown;
        threshold = capacity * 3 / 4;
    }

    /** Maps key to value; returns the value it mapped to before, or null. */
    public synchronized Object put(Object key, Object value) {
        if (value == null) {
            throw new NullPointerException();
        }
        Entry e = find(key);
        if (e != null) {
            Object old = e.value;
            e.value = value;
            return old;
        }
        if (count >= threshold) {
            rehash();
        }
        int hash = key.hashCode();
        int i = index(hash, table.length);
        table[i] = new Entry(hash, key, value, table[i]);
        count++;
        return null;
    }

    /** Removes key's mapping; returns the value it had, or null. */
    public synchronized Object remove(Object key) {
        int hash = key.hashCode();
        int i = index(hash, table.length);
        for (Entry e = table[i], previous = null; e != null; previous = e, e = e.next) {
            if (e.hash == hash && key.equals(e.key)) {
                if (previous != null) {
                    previous.next = e.next;
                } else {
                    table[i] = e.next;
                }
                count--;
                return e.value;
            }
        }
        return null;
    }

    public synchronized void clear() {
        for (int i = 0; i < table.length; i++) {
            table[i] = null;
        }
        count = 0;
    }

    /** "{key=value, key=value}". */
    public synchronized String toString() {
        StringBuffer sb = new StringBuffer("{");
        boolean first = true;
        for (int i = table.length - 1; i >= 0; i--) {
            for (Entry e = table[i]; e != null; e = e.next) {
                if (!first) {
                    sb.append(", ");
                }
                first = false;
                sb.append(e.key == this ? "(this Map)" : String.valueOf(e.key)).append('=');
                sb.append(e.value == this ? "(this Map)" : String.valueOf(e.value));
            }
        }
        return sb.append('}').toString();
    }

    private static final class Entry {
        final int hash;
        final Object key;
        Object value;
        Entry next;

        Entry(int hash, Object key, Object value, Entry next) {
            this.hash = hash;
            this.key = key;
            this.value = value;
            this.next = next;
        }
    }

    /** Enumerates a copy, so the table may change meanwhile. */
    private static final class Snapshot implements Enumeration {
        private final Object[] items;
        private int next;

        Snapshot(Object[] items) {
            this.items = items;
        }

        public boolean hasMoreElements() {
            return next < items.length;
        }

        public Object nextElement() {
            if (next >= items.length) {
                throw new NoSuchElementException("Hashtable Enumerator");
            }
            return items[next++];
        }
    }
}
