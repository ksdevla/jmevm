package java.io;

/** Reads the bytes of an array. */
public class ByteArrayInputStream extends InputStream {
    protected byte[] buf;
    /** The index of the next byte to read. */
    protected int pos;
    /** Where reset() goes back to. */
    protected int mark;
    /** One past the last byte to read. */
    protected int count;

    public ByteArrayInputStream(byte[] buf) {
        this.buf = buf;
        this.count = buf.length;
    }

    public ByteArrayInputStream(byte[] buf, int offset, int length) {
        this.buf = buf;
        this.pos = offset;
        this.mark = offset;
        this.count = Math.min(offset + length, buf.length);
    }

    public synchronized int read() {
        return pos < count ? buf[pos++] & 0xff : -1;
    }

    public synchronized int read(byte[] b, int off, int len) {
        checkRange(b.length, off, len);
        if (pos >= count) {
            return -1;
        }
        int n = Math.min(len, count - pos);
        System.arraycopy(buf, pos, b, off, n);
        pos += n;
        return n;
    }

    public synchronized long skip(long n) {
        long k = Math.max(0, Math.min(n, count - pos));
        pos += (int) k;
        return k;
    }

    public synchronized int available() {
        return count - pos;
    }

    public boolean markSupported() {
        return true;
    }

    public synchronized void mark(int readlimit) {
        mark = pos;
    }

    public synchronized void reset() {
        pos = mark;
    }

    public synchronized void close() throws IOException {
    }
}
