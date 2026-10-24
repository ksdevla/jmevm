package jmevm;

import java.io.OutputStream;

/** Bytes to the host's console: standard output or standard error. */
public final class ConsoleOutputStream extends OutputStream {
    public static final int OUT = 1;
    public static final int ERR = 2;

    private final int stream;

    public ConsoleOutputStream(int stream) {
        this.stream = stream;
    }

    public void write(int b) {
        write0(stream, b);
    }

    public void write(byte[] b, int off, int len) {
        if (off < 0 || len < 0 || len > b.length - off) {
            throw new IndexOutOfBoundsException();
        }
        writeBytes0(stream, b, off, len);
    }

    private static native void write0(int stream, int b);

    private static native void writeBytes0(int stream, byte[] b, int off, int len);
}
