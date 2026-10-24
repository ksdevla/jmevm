package java.io;

/** Collects bytes in an array that grows as needed. */
public class ByteArrayOutputStream extends OutputStream {
    protected byte[] buf;
    /** How many bytes of buf are written. */
    protected int count;

    public ByteArrayOutputStream() {
        this(32);
    }

    public ByteArrayOutputStream(int size) {
        if (size < 0) {
            throw new IllegalArgumentException("Negative initial size: " + size);
        }
        buf = new byte[size];
    }

    private void ensureCapacity(int capacity) {
        if (capacity > buf.length) {
            byte[] bigger = new byte[Math.max(capacity, buf.length * 2)];
            System.arraycopy(buf, 0, bigger, 0, count);
            buf = bigger;
        }
    }

    public synchronized void write(int b) {
        ensureCapacity(count + 1);
        buf[count++] = (byte) b;
    }

    public synchronized void write(byte[] b, int off, int len) {
        InputStream.checkRange(b.length, off, len);
        ensureCapacity(count + len);
        System.arraycopy(b, off, buf, count, len);
        count += len;
    }

    /** Forgets the bytes written, keeping the array for the next ones. */
    public synchronized void reset() {
        count = 0;
    }

    public synchronized byte[] toByteArray() {
        byte[] copy = new byte[count];
        System.arraycopy(buf, 0, copy, 0, count);
        return copy;
    }

    public int size() {
        return count;
    }

    /** The bytes decoded in the default character encoding. */
    public String toString() {
        return new String(buf, 0, count);
    }

    public synchronized void close() throws IOException {
    }
}
