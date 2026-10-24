package java.io;

/** A source of chars. Subclasses implement read(char[], int, int) and close(). */
public abstract class Reader {
    /** The object whose lock the reader's operations hold. */
    protected Object lock;

    protected Reader() {
        lock = this;
    }

    protected Reader(Object lock) {
        if (lock == null) {
            throw new NullPointerException();
        }
        this.lock = lock;
    }

    /** The next char, or -1 at the end of the stream. */
    public int read() throws IOException {
        char[] c = new char[1];
        return read(c, 0, 1) < 0 ? -1 : c[0];
    }

    public int read(char[] cbuf) throws IOException {
        return read(cbuf, 0, cbuf.length);
    }

    /** Reads up to len chars into cbuf at off; returns how many, or -1 at the end. */
    public abstract int read(char[] cbuf, int off, int len) throws IOException;

    public long skip(long n) throws IOException {
        if (n < 0) {
            throw new IllegalArgumentException("skip value is negative");
        }
        char[] scratch = new char[(int) Math.min(n, 512)];
        long left = n;
        synchronized (lock) {
            while (left > 0) {
                int r = read(scratch, 0, (int) Math.min(left, scratch.length));
                if (r < 0) {
                    break;
                }
                left -= r;
            }
        }
        return n - left;
    }

    /** Whether read() would not block. */
    public boolean ready() throws IOException {
        return false;
    }

    public boolean markSupported() {
        return false;
    }

    public void mark(int readAheadLimit) throws IOException {
        throw new IOException("mark() not supported");
    }

    public void reset() throws IOException {
        throw new IOException("reset() not supported");
    }

    public abstract void close() throws IOException;
}
