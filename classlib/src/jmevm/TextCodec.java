package jmevm;

import java.io.UnsupportedEncodingException;

/**
 * Converts between bytes and characters in one character encoding. Each
 * conversion (a string, a reader, a writer) gets its own instance, as some
 * encodings keep state (UTF-16's byte order mark).
 *
 * <p>Encodings: ISO-8859-1 (the default, as microedition.encoding says),
 * US-ASCII, UTF-8, UTF-16 (with a byte order mark), UTF-16BE, UTF-16LE,
 * windows-1251 and windows-1252. Malformed input decodes to U+FFFD;
 * characters an encoding lacks encode as '?'.
 */
public abstract class TextCodec {
    /** decodeOne(): the bytes end inside a character; more are needed. */
    public static final int NEED_MORE = -1;
    /** decodeOne()'s code point for bytes that are no character (a byte order mark). */
    public static final int NOTHING = 0x110000;
    /** The most bytes encodeOne() writes for one code point (with a byte order mark). */
    public static final int MAX_BYTES = 6;

    private static final int REPLACEMENT = 0xfffd;
    private static String defaultName;

    TextCodec() {
    }

    /** A codec for an encoding name, in any letter case. */
    public static TextCodec forName(String name) throws UnsupportedEncodingException {
        TextCodec codec = lookup(name);
        if (codec == null) {
            throw new UnsupportedEncodingException(name);
        }
        return codec;
    }

    /** A codec for the platform's default encoding. */
    public static TextCodec forDefault() {
        if (defaultName == null) {
            String name = System.getProperty("microedition.encoding");
            defaultName = name != null && lookup(name) != null ? name : "ISO-8859-1";
        }
        return lookup(defaultName);
    }

    private static TextCodec lookup(String name) {
        if (is(name, "ISO-8859-1", "ISO8859_1", "ISO8859-1", "ISO_8859_1", "8859_1", "LATIN1", "ISO-LATIN-1")) {
            return new SingleByte(null, 0x100);
        }
        if (is(name, "US-ASCII", "ASCII", "US_ASCII", null, null, null, null)) {
            return new SingleByte(null, 0x80);
        }
        if (is(name, "UTF-8", "UTF8", null, null, null, null, null)) {
            return new Utf8();
        }
        if (is(name, "UTF-16", "UTF16", "UTF_16", null, null, null, null)) {
            return new Utf16(Utf16.UNKNOWN);
        }
        if (is(name, "UTF-16BE", "UTF_16BE", "UnicodeBigUnmarked", null, null, null, null)) {
            return new Utf16(Utf16.BIG);
        }
        if (is(name, "UTF-16LE", "UTF_16LE", "UnicodeLittleUnmarked", null, null, null, null)) {
            return new Utf16(Utf16.LITTLE);
        }
        if (is(name, "windows-1251", "Cp1251", null, null, null, null, null)) {
            return new SingleByte(CP1251, 0x100);
        }
        if (is(name, "windows-1252", "Cp1252", null, null, null, null, null)) {
            return new SingleByte(CP1252, 0x100);
        }
        return null;
    }

    private static boolean is(String name, String a, String b, String c, String d, String e, String f, String g) {
        return name.equalsIgnoreCase(a) || (b != null && name.equalsIgnoreCase(b))
                || (c != null && name.equalsIgnoreCase(c)) || (d != null && name.equalsIgnoreCase(d))
                || (e != null && name.equalsIgnoreCase(e)) || (f != null && name.equalsIgnoreCase(f))
                || (g != null && name.equalsIgnoreCase(g));
    }

    /**
     * Decodes the character that starts at b[pos], looking no further than
     * end. Returns (code point << 3) | bytes used, where the code point may
     * be NOTHING, or NEED_MORE if the bytes up to end are an incomplete but
     * valid start of a character.
     */
    public abstract int decodeOne(byte[] b, int pos, int end);

    /** Encodes a code point (a lone surrogate is malformed) into out[pos...]; returns the bytes written. */
    public abstract int encodeOne(int codePoint, byte[] out, int pos);

    /** All of b[off..off+len) as characters. */
    public final char[] decode(byte[] b, int off, int len) {
        if (off < 0 || len < 0 || len > b.length - off) {
            throw new IndexOutOfBoundsException();
        }
        char[] out = new char[len];   /* no encoding gives more chars than bytes */
        int n = 0;
        int end = off + len;
        for (int pos = off; pos < end;) {
            int r = decodeOne(b, pos, end);
            if (r == NEED_MORE) {
                out[n++] = (char) REPLACEMENT;   /* cut off at the end */
                break;
            }
            pos += r & 7;
            if (r >> 3 != NOTHING) {
                n = putChar(r >> 3, out, n);
            }
        }
        return n == out.length ? out : copyOf(out, n);
    }

    /** All of c[off..off+len) as bytes. */
    public final byte[] encode(char[] c, int off, int len) {
        if (off < 0 || len < 0 || len > c.length - off) {
            throw new IndexOutOfBoundsException();
        }
        byte[] out = new byte[len + MAX_BYTES];
        int n = 0;
        int end = off + len;
        for (int i = off; i < end; i++) {
            int cp = c[i];
            if (isHighSurrogate(cp) && i + 1 < end && isLowSurrogate(c[i + 1])) {
                cp = toCodePoint(cp, c[++i]);
            }
            if (out.length - n < MAX_BYTES) {
                byte[] bigger = new byte[out.length * 2];
                System.arraycopy(out, 0, bigger, 0, n);
                out = bigger;
            }
            n += encodeOne(cp, out, n);
        }
        byte[] exact = new byte[n];
        System.arraycopy(out, 0, exact, 0, n);
        return exact;
    }

    /** Stores a code point as one char, or two (a surrogate pair); returns the new length. */
    public static int putChar(int codePoint, char[] out, int n) {
        if (codePoint < 0x10000) {
            out[n++] = (char) codePoint;
        } else {
            codePoint -= 0x10000;
            out[n++] = (char) (0xd800 + (codePoint >> 10));
            out[n++] = (char) (0xdc00 + (codePoint & 0x3ff));
        }
        return n;
    }

    public static boolean isHighSurrogate(int c) {
        return c >= 0xd800 && c <= 0xdbff;
    }

    public static boolean isLowSurrogate(int c) {
        return c >= 0xdc00 && c <= 0xdfff;
    }

    public static int toCodePoint(int high, int low) {
        return 0x10000 + ((high - 0xd800) << 10) + (low - 0xdc00);
    }

    private static char[] copyOf(char[] c, int n) {
        char[] r = new char[n];
        System.arraycopy(c, 0, r, 0, n);
        return r;
    }

    private static int malformed(int length) {
        return (REPLACEMENT << 3) | length;
    }

    /** One byte a character: ASCII, ISO-8859-1, or a table for 0x80 to 0xFF. */
    private static final class SingleByte extends TextCodec {
        private final String upper;   /* chars of bytes 0x80 to 0xFF; null: the same values */
        private final int limit;      /* bytes from here are not characters (ASCII: 0x80) */

        SingleByte(String upper, int limit) {
            this.upper = upper;
            this.limit = limit;
        }

        public int decodeOne(byte[] b, int pos, int end) {
            int v = b[pos] & 0xff;
            if (v >= limit) {
                return malformed(1);
            }
            return ((v < 0x80 || upper == null ? v : upper.charAt(v - 0x80)) << 3) | 1;
        }

        public int encodeOne(int codePoint, byte[] out, int pos) {
            int v = '?';
            if (codePoint < 0x80 || (upper == null && codePoint < limit)) {
                v = codePoint;
            } else if (upper != null && codePoint != REPLACEMENT) {
                int i = upper.indexOf(codePoint);
                if (i >= 0) {
                    v = 0x80 + i;
                }
            }
            out[pos] = (byte) v;
            return 1;
        }
    }

    /** UTF-8, where a malformed sequence decodes to one U+FFFD per maximal valid prefix. */
    private static final class Utf8 extends TextCodec {
        public int decodeOne(byte[] b, int pos, int end) {
            int b0 = b[pos] & 0xff;
            if (b0 < 0x80) {
                return (b0 << 3) | 1;
            }
            int length;
            int lo = 0x80;   /* the range of the second byte */
            int hi = 0xbf;
            if (b0 >= 0xc2 && b0 <= 0xdf) {
                length = 2;
            } else if (b0 >= 0xe0 && b0 <= 0xef) {
                length = 3;
                if (b0 == 0xe0) {
                    lo = 0xa0;   /* no overlong forms */
                }
            } else if (b0 >= 0xf0 && b0 <= 0xf4) {
                length = 4;
                if (b0 == 0xf0) {
                    lo = 0x90;
                } else if (b0 == 0xf4) {
                    hi = 0x8f;   /* nothing above U+10FFFF */
                }
            } else {
                return malformed(1);
            }
            int cp = b0 & (0x7f >> length);
            for (int i = 1; i < length; i++) {
                if (pos + i >= end) {
                    return NEED_MORE;
                }
                int v = b[pos + i] & 0xff;
                if (v < lo || v > hi) {
                    return malformed(i);
                }
                lo = 0x80;
                hi = 0xbf;
                cp = (cp << 6) | (v & 0x3f);
            }
            if (cp >= 0xd800 && cp <= 0xdfff) {
                return malformed(3);   /* an encoded surrogate: one malformed character */
            }
            return (cp << 3) | length;
        }

        public int encodeOne(int cp, byte[] out, int pos) {
            if (cp < 0x80) {
                out[pos] = (byte) cp;
                return 1;
            }
            if (cp < 0x800) {
                out[pos] = (byte) (0xc0 | (cp >> 6));
                out[pos + 1] = (byte) (0x80 | (cp & 0x3f));
                return 2;
            }
            if (cp < 0x10000) {
                if (cp >= 0xd800 && cp <= 0xdfff) {
                    out[pos] = '?';
                    return 1;
                }
                out[pos] = (byte) (0xe0 | (cp >> 12));
                out[pos + 1] = (byte) (0x80 | ((cp >> 6) & 0x3f));
                out[pos + 2] = (byte) (0x80 | (cp & 0x3f));
                return 3;
            }
            out[pos] = (byte) (0xf0 | (cp >> 18));
            out[pos + 1] = (byte) (0x80 | ((cp >> 12) & 0x3f));
            out[pos + 2] = (byte) (0x80 | ((cp >> 6) & 0x3f));
            out[pos + 3] = (byte) (0x80 | (cp & 0x3f));
            return 4;
        }
    }

    /**
     * UTF-16 in a byte order. With the order unknown (plain "UTF-16"),
     * decoding takes it from a byte order mark, big-endian without one, and
     * encoding writes a big-endian mark first.
     */
    private static final class Utf16 extends TextCodec {
        static final int UNKNOWN = 0;
        static final int BIG = 1;
        static final int LITTLE = 2;

        private int order;

        Utf16(int order) {
            this.order = order;
        }

        private int unit(byte[] b, int pos) {
            int x = b[pos] & 0xff;
            int y = b[pos + 1] & 0xff;
            return order == LITTLE ? (y << 8) | x : (x << 8) | y;
        }

        public int decodeOne(byte[] b, int pos, int end) {
            if (end - pos < 2) {
                return NEED_MORE;
            }
            if (order == UNKNOWN) {
                int mark = ((b[pos] & 0xff) << 8) | (b[pos + 1] & 0xff);
                order = mark == 0xfffe ? LITTLE : BIG;
                if (mark == 0xfeff || mark == 0xfffe) {
                    return (NOTHING << 3) | 2;
                }
            }
            int u = unit(b, pos);
            if (isLowSurrogate(u)) {
                return malformed(2);
            }
            if (!isHighSurrogate(u)) {
                return (u << 3) | 2;
            }
            if (end - pos < 4) {
                return NEED_MORE;
            }
            int low = unit(b, pos + 2);
            if (!isLowSurrogate(low)) {
                return malformed(2);
            }
            return (toCodePoint(u, low) << 3) | 4;
        }

        public int encodeOne(int cp, byte[] out, int pos) {
            int n = 0;
            if (order == UNKNOWN) {
                order = BIG;
                n = put(0xfeff, out, pos);
            }
            if (cp >= 0x10000) {
                cp -= 0x10000;
                n += put(0xd800 + (cp >> 10), out, pos + n);
                return n + put(0xdc00 + (cp & 0x3ff), out, pos + n);
            }
            return n + put(cp >= 0xd800 && cp <= 0xdfff ? REPLACEMENT : cp, out, pos + n);
        }

        private int put(int unit, byte[] out, int pos) {
            if (order == LITTLE) {
                out[pos] = (byte) unit;
                out[pos + 1] = (byte) (unit >> 8);
            } else {
                out[pos] = (byte) (unit >> 8);
                out[pos + 1] = (byte) unit;
            }
            return 2;
        }
    }

    private static final String CP1251 = ""
            + "ЂЃ‚ѓ„…†‡€‰Љ‹ЊЌЋЏ"
            + "ђ‘’“”•–—�™љ›њќћџ"
            + " ЎўЈ¤Ґ¦§Ё©Є«¬­®Ї"
            + "°±Ііґµ¶·ё№є»јЅѕї"
            + "АБВГДЕЖЗИЙКЛМНОП"
            + "РСТУФХЦЧШЩЪЫЬЭЮЯ"
            + "абвгдежзийклмноп"
            + "рстуфхцчшщъыьэюя";

    private static final String CP1252 = ""
            + "€�‚ƒ„…†‡ˆ‰Š‹Œ�Ž�"
            + "�‘’“”•–—˜™š›œ�žŸ"
            + " ¡¢£¤¥¦§¨©ª«¬­®¯"
            + "°±²³´µ¶·¸¹º»¼½¾¿"
            + "ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏ"
            + "ÐÑÒÓÔÕÖ×ØÙÚÛÜÝÞß"
            + "àáâãäåæçèéêëìíîï"
            + "ðñòóôõö÷øùúûüýþÿ";
}
