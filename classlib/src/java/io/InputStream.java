package java.io;

/** A source of bytes. Subclasses implement read() and override the rest for speed. */
public abstract class InputStream {
    public InputStream() {
    }

    /** The next byte (0 to 255), or -1 at the end of the stream. */
    public abstract int read() throws IOException;

    public int read(byte[] b) throws IOException {
        return read(b, 0, b.length);
    }

    /**
     * Reads up to len bytes into b at off; returns how many, or -1 at the end
     * of the stream. An error after the first byte ends the read early.
     */
    public int read(byte[] b, int off, int len) throws IOException {
        checkRange(b.length, off, len);
        if (len == 0) {
            return 0;
        }
        int c = read();
        if (c < 0) {
            return -1;
        }
        b[off] = (byte) c;
        int n = 1;
        try {
            while (n < len && (c = read()) >= 0) {
                b[off + n++] = (byte) c;
            }
        } catch (IOException e) {
            /* report the bytes read so far; the error will come again */
        }
        return n;
    }

    static void checkRange(int length, int off, int len) {
        if (off < 0 || len < 0 || len > length - off) {
            throw new IndexOutOfBoundsException("offset " + off + ", count " + len + ", length " + length);
        }
    }

    /** Skips up to n bytes by reading them; returns how many. */
    public long skip(long n) throws IOException {
        if (n <= 0) {
            return 0;
        }
        byte[] scratch = new byte[(int) Math.min(n, 512)];
        long left = n;
        while (left > 0) {
            int r = read(scratch, 0, (int) Math.min(left, scratch.length));
            if (r < 0) {
                break;
            }
            left -= r;
        }
        return n - left;
    }

    /** How many bytes can be read now without blocking (a lower bound). */
    public int available() throws IOException {
        return 0;
    }

    public void close() throws IOException {
    }

    public synchronized void mark(int readlimit) {
    }

    public synchronized void reset() throws IOException {
        throw new IOException("mark/reset not supported");
    }

    public boolean markSupported() {
        return false;
    }
}
