package java.io;

import jmevm.TextCodec;

/**
 * Encodes chars into the bytes of a stream, in a character encoding. Each
 * write goes to the stream at once, but for the first half of a surrogate
 * pair, which waits for its second.
 */
public class OutputStreamWriter extends Writer {
    private OutputStream out;
    private final TextCodec codec;
    private final byte[] bytes = new byte[256];
    private char high;   /* a first half of a surrogate pair, or 0 */

    /** Writes in the default encoding (ISO-8859-1). */
    public OutputStreamWriter(OutputStream os) {
        super(os);
        out = os;
        codec = TextCodec.forDefault();
    }

    public OutputStreamWriter(OutputStream os, String enc) throws UnsupportedEncodingException {
        super(os);
        out = os;
        codec = TextCodec.forName(enc);
    }

    private void ensureOpen() throws IOException {
        if (out == null) {
            throw new IOException("Stream closed");
        }
    }

    public void write(int c) throws IOException {
        write(new char[] {(char) c}, 0, 1);
    }

    public void write(char[] cbuf, int off, int len) throws IOException {
        synchronized (lock) {
            ensureOpen();
            InputStream.checkRange(cbuf.length, off, len);
            int n = 0;
            for (int i = off; i < off + len; i++) {
                char c = cbuf[i];
                int cp;
                if (high != 0) {
                    boolean pair = TextCodec.isLowSurrogate(c);
                    cp = pair ? TextCodec.toCodePoint(high, c) : high;
                    if (!pair) {
                        i--;   /* encode the lone half, then c on its own */
                    }
                    high = 0;
                } else if (TextCodec.isHighSurrogate(c)) {
                    high = c;
                    continue;
                } else {
                    cp = c;
                }
                if (bytes.length - n < TextCodec.MAX_BYTES) {
                    out.write(bytes, 0, n);
                    n = 0;
                }
                n += codec.encodeOne(cp, bytes, n);
            }
            out.write(bytes, 0, n);
        }
    }

    public void write(String str, int off, int len) throws IOException {
        char[] chars = new char[len];
        str.getChars(off, off + len, chars, 0);
        write(chars, 0, len);
    }

    public void flush() throws IOException {
        synchronized (lock) {
            ensureOpen();
            out.flush();
        }
    }

    /** Writes a waiting half of a surrogate pair (as malformed), then closes the stream. */
    public void close() throws IOException {
        synchronized (lock) {
            if (out == null) {
                return;
            }
            if (high != 0) {
                int n = codec.encodeOne(high, bytes, 0);
                high = 0;
                out.write(bytes, 0, n);
            }
            out.close();
            out = null;
        }
    }
}
