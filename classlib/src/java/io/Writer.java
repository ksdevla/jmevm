package java.io;

/** A sink for chars. Subclasses implement write(char[], int, int), flush() and close(). */
public abstract class Writer {
    /** The object whose lock the writer's operations hold. */
    protected Object lock;

    protected Writer() {
        lock = this;
    }

    protected Writer(Object lock) {
        if (lock == null) {
            throw new NullPointerException();
        }
        this.lock = lock;
    }

    /** Writes the low sixteen bits of c as a char. */
    public void write(int c) throws IOException {
        write(new char[] {(char) c}, 0, 1);
    }

    public void write(char[] cbuf) throws IOException {
        write(cbuf, 0, cbuf.length);
    }

    public abstract void write(char[] cbuf, int off, int len) throws IOException;

    public void write(String str) throws IOException {
        write(str, 0, str.length());
    }

    public void write(String str, int off, int len) throws IOException {
        char[] chars = new char[len];
        str.getChars(off, off + len, chars, 0);
        write(chars, 0, len);
    }

    public abstract void flush() throws IOException;

    public abstract void close() throws IOException;
}
