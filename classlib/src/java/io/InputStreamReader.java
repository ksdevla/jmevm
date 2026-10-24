package java.io;

import jmevm.TextCodec;

/** Decodes the bytes of a stream into chars, in a character encoding. */
public class InputStreamReader extends Reader {
    private InputStream in;
    private final TextCodec codec;
    private final byte[] bytes = new byte[256];
    private int start;      /* the bytes read but not decoded: bytes[start..end) */
    private int end;
    private char pending;   /* the second half of a surrogate pair, or 0 */

    /** Reads in the default encoding (ISO-8859-1). */
    public InputStreamReader(InputStream is) {
        super(is);
        in = is;
        codec = TextCodec.forDefault();
    }

    public InputStreamReader(InputStream is, String enc) throws UnsupportedEncodingException {
        super(is);
        in = is;
        codec = TextCodec.forName(enc);
    }

    private void ensureOpen() throws IOException {
        if (in == null) {
            throw new IOException("Stream closed");
        }
    }

    /** Reads more bytes after those not decoded yet; false at the end of the stream. */
    private boolean fill() throws IOException {
        if (start > 0) {
            System.arraycopy(bytes, start, bytes, 0, end - start);
            end -= start;
            start = 0;
        }
        int n = in.read(bytes, end, bytes.length - end);
        if (n < 0) {
            return false;
        }
        end += n;
        return true;
    }

    public int read() throws IOException {
        char[] c = new char[1];
        return read(c, 0, 1) < 0 ? -1 : c[0];
    }

    /** Reads what is decodable; it blocks only while it has no char at all. */
    public int read(char[] cbuf, int off, int len) throws IOException {
        synchronized (lock) {
            ensureOpen();
            InputStream.checkRange(cbuf.length, off, len);
            int n = 0;
            while (n < len) {
                if (pending != 0) {
                    cbuf[off + n++] = pending;
                    pending = 0;
                    continue;
                }
                int r = start < end ? codec.decodeOne(bytes, start, end) : TextCodec.NEED_MORE;
                if (r == TextCodec.NEED_MORE) {
                    if (n > 0) {
                        break;
                    }
                    if (!fill()) {
                        if (start == end) {
                            return -1;
                        }
                        start = end;   /* the stream ends inside a character */
                        cbuf[off + n++] = '�';
                    }
                    continue;
                }
                start += r & 7;
                int cp = r >> 3;
                if (cp >= 0x10000 && cp != TextCodec.NOTHING) {
                    char[] pair = new char[2];
                    TextCodec.putChar(cp, pair, 0);
                    cbuf[off + n++] = pair[0];
                    pending = pair[1];
                } else if (cp != TextCodec.NOTHING) {
                    cbuf[off + n++] = (char) cp;
                }
            }
            return n;
        }
    }

    public boolean ready() throws IOException {
        synchronized (lock) {
            ensureOpen();
            return pending != 0 || start < end || in.available() > 0;
        }
    }

    public void close() throws IOException {
        synchronized (lock) {
            if (in != null) {
                in.close();
                in = null;
            }
        }
    }
}
