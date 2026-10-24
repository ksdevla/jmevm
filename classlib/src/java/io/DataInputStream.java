package java.io;

/** Reads primitive types and modified UTF-8 strings from another stream. */
public class DataInputStream extends InputStream implements DataInput {
    protected InputStream in;

    public DataInputStream(InputStream in) {
        this.in = in;
    }

    public int read() throws IOException {
        return in.read();
    }

    public final int read(byte[] b) throws IOException {
        return in.read(b, 0, b.length);
    }

    public final int read(byte[] b, int off, int len) throws IOException {
        return in.read(b, off, len);
    }

    public final void readFully(byte[] b) throws IOException {
        readFully(b, 0, b.length);
    }

    /** Reads exactly len bytes, or throws EOFException. */
    public final void readFully(byte[] b, int off, int len) throws IOException {
        checkRange(b.length, off, len);
        while (len > 0) {
            int n = in.read(b, off, len);
            if (n < 0) {
                throw new EOFException();
            }
            off += n;
            len -= n;
        }
    }

    public final int skipBytes(int n) throws IOException {
        int total = 0;
        while (total < n) {
            long skipped = in.skip(n - total);
            if (skipped <= 0) {
                break;
            }
            total += (int) skipped;
        }
        return total;
    }

    /** The next byte, or EOFException at the end of the stream. */
    private int next() throws IOException {
        int b = in.read();
        if (b < 0) {
            throw new EOFException();
        }
        return b;
    }

    public final boolean readBoolean() throws IOException {
        return next() != 0;
    }

    public final byte readByte() throws IOException {
        return (byte) next();
    }

    public final int readUnsignedByte() throws IOException {
        return next();
    }

    public final short readShort() throws IOException {
        return (short) readUnsignedShort();
    }

    public final int readUnsignedShort() throws IOException {
        int hi = next();
        return (hi << 8) | next();
    }

    public final char readChar() throws IOException {
        return (char) readUnsignedShort();
    }

    public final int readInt() throws IOException {
        int hi = readUnsignedShort();
        return (hi << 16) | readUnsignedShort();
    }

    public final long readLong() throws IOException {
        long hi = readInt();
        return (hi << 32) | (readInt() & 0xffffffffL);
    }

    public final float readFloat() throws IOException {
        return Float.intBitsToFloat(readInt());
    }

    public final double readDouble() throws IOException {
        return Double.longBitsToDouble(readLong());
    }

    public final String readUTF() throws IOException {
        return readUTF(this);
    }

    /**
     * Reads a string written by writeUTF: a two-byte length, then that many
     * bytes of modified UTF-8 (U+0000 as two bytes, no four-byte forms).
     */
    public static final String readUTF(DataInput in) throws IOException {
        int length = in.readUnsignedShort();
        byte[] bytes = new byte[length];
        in.readFully(bytes, 0, length);
        char[] chars = new char[length];
        int n = 0;
        for (int i = 0; i < length;) {
            int b = bytes[i] & 0xff;
            int size = b < 0x80 ? 1 : (b >> 5) == 6 ? 2 : (b >> 4) == 14 ? 3 : 0;
            if (size == 0) {
                throw new UTFDataFormatException("malformed input around byte " + i);
            }
            if (i + size > length) {
                throw new UTFDataFormatException("malformed input: partial character at end");
            }
            int c = b & (0xff >> (size + (size > 1 ? 1 : 0)));
            for (int k = 1; k < size; k++) {
                int more = bytes[i + k] & 0xff;
                if ((more & 0xc0) != 0x80) {
                    throw new UTFDataFormatException("malformed input around byte " + (i + k));
                }
                c = (c << 6) | (more & 0x3f);
            }
            chars[n++] = (char) c;
            i += size;
        }
        return new String(chars, 0, n);
    }

    public long skip(long n) throws IOException {
        return in.skip(n);
    }

    public int available() throws IOException {
        return in.available();
    }

    public void close() throws IOException {
        in.close();
    }

    public synchronized void mark(int readlimit) {
        in.mark(readlimit);
    }

    public synchronized void reset() throws IOException {
        in.reset();
    }

    public boolean markSupported() {
        return in.markSupported();
    }
}
