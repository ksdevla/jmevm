package java.io;

/** Writes primitive types and modified UTF-8 strings to another stream. */
public class DataOutputStream extends OutputStream implements DataOutput {
    protected OutputStream out;

    public DataOutputStream(OutputStream out) {
        this.out = out;
    }

    public void write(int b) throws IOException {
        out.write(b);
    }

    public void write(byte[] b, int off, int len) throws IOException {
        out.write(b, off, len);
    }

    public void flush() throws IOException {
        out.flush();
    }

    public void close() throws IOException {
        out.close();
    }

    public final void writeBoolean(boolean v) throws IOException {
        out.write(v ? 1 : 0);
    }

    public final void writeByte(int v) throws IOException {
        out.write(v);
    }

    public final void writeShort(int v) throws IOException {
        out.write(v >> 8);
        out.write(v);
    }

    public final void writeChar(int v) throws IOException {
        writeShort(v);
    }

    public final void writeInt(int v) throws IOException {
        byte[] b = {(byte) (v >> 24), (byte) (v >> 16), (byte) (v >> 8), (byte) v};
        out.write(b, 0, 4);
    }

    public final void writeLong(long v) throws IOException {
        writeInt((int) (v >> 32));
        writeInt((int) v);
    }

    public final void writeFloat(float v) throws IOException {
        writeInt(Float.floatToIntBits(v));
    }

    public final void writeDouble(double v) throws IOException {
        writeLong(Double.doubleToLongBits(v));
    }

    /** Writes each char as two bytes. */
    public final void writeChars(String s) throws IOException {
        int n = s.length();
        byte[] b = new byte[n * 2];
        for (int i = 0; i < n; i++) {
            char c = s.charAt(i);
            b[2 * i] = (byte) (c >> 8);
            b[2 * i + 1] = (byte) c;
        }
        out.write(b, 0, b.length);
    }

    /**
     * Writes a two-byte length, then the string in modified UTF-8: chars
     * U+0001 to U+007F as one byte, U+0000 and up to U+07FF as two, the rest
     * as three (each half of a surrogate pair on its own).
     */
    public final void writeUTF(String s) throws IOException {
        int n = s.length();
        int length = 0;
        for (int i = 0; i < n; i++) {
            char c = s.charAt(i);
            length += c >= 1 && c < 0x80 ? 1 : c < 0x800 ? 2 : 3;
        }
        if (length > 0xffff) {
            throw new UTFDataFormatException("encoded string too long: " + length + " bytes");
        }
        byte[] b = new byte[length + 2];
        b[0] = (byte) (length >> 8);
        b[1] = (byte) length;
        int p = 2;
        for (int i = 0; i < n; i++) {
            char c = s.charAt(i);
            if (c >= 1 && c < 0x80) {
                b[p++] = (byte) c;
            } else if (c < 0x800) {
                b[p++] = (byte) (0xc0 | (c >> 6));
                b[p++] = (byte) (0x80 | (c & 0x3f));
            } else {
                b[p++] = (byte) (0xe0 | (c >> 12));
                b[p++] = (byte) (0x80 | ((c >> 6) & 0x3f));
                b[p++] = (byte) (0x80 | (c & 0x3f));
            }
        }
        out.write(b, 0, b.length);
    }
}
