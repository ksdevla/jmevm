package java.io;

/** A sink for bytes. Subclasses implement write(int) and override the rest for speed. */
public abstract class OutputStream {
    public OutputStream() {
    }

    /** Writes the low eight bits of b. */
    public abstract void write(int b) throws IOException;

    public void write(byte[] b) throws IOException {
        write(b, 0, b.length);
    }

    public void write(byte[] b, int off, int len) throws IOException {
        InputStream.checkRange(b.length, off, len);
        for (int i = 0; i < len; i++) {
            write(b[off + i]);
        }
    }

    public void flush() throws IOException {
    }

    public void close() throws IOException {
    }
}
