package java.io;

/**
 * Prints values as text, in the default encoding, to another stream. It
 * never throws an IOException: an error sets a flag checkError() reports.
 */
public class PrintStream extends OutputStream {
    private OutputStream out;
    private boolean error;

    public PrintStream(OutputStream out) {
        if (out == null) {
            throw new NullPointerException();
        }
        this.out = out;
    }

    public synchronized void flush() {
        if (out != null) {
            try {
                out.flush();
            } catch (IOException e) {
                error = true;
            }
        }
    }

    public synchronized void close() {
        if (out != null) {
            flush();
            try {
                out.close();
            } catch (IOException e) {
                error = true;
            }
            out = null;
        }
    }

    /** Flushes, then tells whether any operation failed (or the stream is closed). */
    public boolean checkError() {
        flush();
        return error || out == null;
    }

    protected void setError() {
        error = true;
    }

    public synchronized void write(int b) {
        if (out == null) {
            error = true;
            return;
        }
        try {
            out.write(b);
        } catch (IOException e) {
            error = true;
        }
    }

    public synchronized void write(byte[] buf, int off, int len) {
        if (out == null) {
            error = true;
            return;
        }
        try {
            out.write(buf, off, len);
        } catch (IOException e) {
            error = true;
        }
    }

    public void print(boolean b) {
        print(b ? "true" : "false");
    }

    public void print(char c) {
        print(String.valueOf(c));
    }

    public void print(int i) {
        print(String.valueOf(i));
    }

    public void print(long l) {
        print(String.valueOf(l));
    }

    public void print(float f) {
        print(String.valueOf(f));
    }

    public void print(double d) {
        print(String.valueOf(d));
    }

    public void print(char[] s) {
        print(new String(s));
    }

    public void print(String s) {
        byte[] b = (s != null ? s : "null").getBytes();
        write(b, 0, b.length);
    }

    public void print(Object obj) {
        print(String.valueOf(obj));
    }

    public void println() {
        write('\n');
    }

    public void println(boolean x) {
        println(x ? "true" : "false");
    }

    public void println(char x) {
        println(String.valueOf(x));
    }

    public void println(int x) {
        println(String.valueOf(x));
    }

    public void println(long x) {
        println(String.valueOf(x));
    }

    public void println(float x) {
        println(String.valueOf(x));
    }

    public void println(double x) {
        println(String.valueOf(x));
    }

    public void println(char[] x) {
        println(new String(x));
    }

    /** Prints the line in one write, so lines of different threads do not mix. */
    public void println(String x) {
        print((x != null ? x : "null") + '\n');
    }

    public void println(Object x) {
        println(String.valueOf(x));
    }
}
