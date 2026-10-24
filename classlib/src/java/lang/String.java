package java.lang;

import java.io.UnsupportedEncodingException;

import jmevm.TextCodec;

/**
 * An immutable sequence of chars. A string is a window (offset, count) on a
 * char array, which substrings share. The virtual machine makes strings
 * itself (literals, names) and reads these three fields by name.
 */
public final class String {
    private static final char[] NO_CHARS = new char[0];

    private final char[] value;
    private final int offset;
    private final int count;
    private int hash;   /* 0: not computed yet */

    public String() {
        value = NO_CHARS;
        offset = 0;
        count = 0;
    }

    public String(String original) {
        value = original.value;
        offset = original.offset;
        count = original.count;
        hash = original.hash;
    }

    public String(char[] value) {
        this(value, 0, value.length);
    }

    public String(char[] value, int offset, int count) {
        checkRange(offset, count, value.length);
        this.value = new char[count];
        System.arraycopy(value, offset, this.value, 0, count);
        this.offset = 0;
        this.count = count;
    }

    public String(byte[] bytes, int off, int len, String enc) throws UnsupportedEncodingException {
        this(TextCodec.forName(enc).decode(bytes, off, len), true);
    }

    public String(byte[] bytes, String enc) throws UnsupportedEncodingException {
        this(bytes, 0, bytes.length, enc);
    }

    public String(byte[] bytes, int off, int len) {
        this(TextCodec.forDefault().decode(bytes, off, len), true);
    }

    public String(byte[] bytes) {
        this(bytes, 0, bytes.length);
    }

    public String(StringBuffer buffer) {
        this(buffer.toString());
    }

    /*
     * Use chars as they are, without a copy: the caller hands them over, or
     * they are another string's. The flag only sets these constructors apart
     * from the copying ones.
     */
    String(char[] chars, boolean shared) {
        this(chars, 0, chars.length, shared);
    }

    String(char[] chars, int offset, int count, boolean shared) {
        value = chars;
        this.offset = offset;
        this.count = count;
    }

    private static void checkRange(int offset, int count, int length) {
        if (offset < 0 || count < 0 || count > length - offset) {
            throw new StringIndexOutOfBoundsException("offset " + offset + ", count " + count + ", length " + length);
        }
    }

    public int length() {
        return count;
    }

    public char charAt(int index) {
        if (index < 0 || index >= count) {
            throw new StringIndexOutOfBoundsException(index);
        }
        return value[offset + index];
    }

    /** Copies the chars from srcBegin up to srcEnd into dst at dstBegin. */
    public void getChars(int srcBegin, int srcEnd, char[] dst, int dstBegin) {
        if (srcBegin < 0) {
            throw new StringIndexOutOfBoundsException(srcBegin);
        }
        if (srcEnd > count) {
            throw new StringIndexOutOfBoundsException(srcEnd);
        }
        if (srcBegin > srcEnd) {
            throw new StringIndexOutOfBoundsException(srcEnd - srcBegin);
        }
        System.arraycopy(value, offset + srcBegin, dst, dstBegin, srcEnd - srcBegin);
    }

    public char[] toCharArray() {
        char[] chars = new char[count];
        System.arraycopy(value, offset, chars, 0, count);
        return chars;
    }

    public byte[] getBytes(String enc) throws UnsupportedEncodingException {
        return TextCodec.forName(enc).encode(value, offset, count);
    }

    public byte[] getBytes() {
        return TextCodec.forDefault().encode(value, offset, count);
    }

    public boolean equals(Object obj) {
        if (obj == this) {
            return true;
        }
        if (!(obj instanceof String)) {
            return false;
        }
        String other = (String) obj;
        return other.count == count && regionMatches(false, 0, other, 0, count);
    }

    public boolean equalsIgnoreCase(String other) {
        return other != null && other.count == count && regionMatches(true, 0, other, 0, count);
    }

    /** Below zero if this sorts before other by char values, zero if equal. */
    public int compareTo(String other) {
        int n = Math.min(count, other.count);
        for (int i = 0; i < n; i++) {
            int d = value[offset + i] - other.value[other.offset + i];
            if (d != 0) {
                return d;
            }
        }
        return count - other.count;
    }

    public boolean regionMatches(int toffset, String other, int ooffset, int len) {
        return regionMatches(false, toffset, other, ooffset, len);
    }

    /** Whether len chars from toffset equal those of other from ooffset. */
    public boolean regionMatches(boolean ignoreCase, int toffset, String other, int ooffset, int len) {
        if (toffset < 0 || ooffset < 0 || toffset > (long) count - len || ooffset > (long) other.count - len) {
            return false;
        }
        char[] a = value;
        char[] b = other.value;
        int i = offset + toffset;
        int j = other.offset + ooffset;
        for (int k = 0; k < len; k++) {
            char c1 = a[i + k];
            char c2 = b[j + k];
            if (c1 == c2) {
                continue;
            }
            if (!ignoreCase) {
                return false;
            }
            char u1 = Character.toUpperCase(c1);
            char u2 = Character.toUpperCase(c2);
            if (u1 != u2 && Character.toLowerCase(u1) != Character.toLowerCase(u2)) {
                return false;
            }
        }
        return true;
    }

    public boolean startsWith(String prefix, int toffset) {
        return regionMatches(false, toffset, prefix, 0, prefix.count);
    }

    public boolean startsWith(String prefix) {
        return regionMatches(false, 0, prefix, 0, prefix.count);
    }

    public boolean endsWith(String suffix) {
        return regionMatches(false, count - suffix.count, suffix, 0, suffix.count);
    }

    /** s[0]*31^(n-1) + s[1]*31^(n-2) + ... + s[n-1]. */
    public int hashCode() {
        int h = hash;
        if (h == 0 && count > 0) {
            for (int i = offset, end = offset + count; i < end; i++) {
                h = 31 * h + value[i];
            }
            hash = h;
        }
        return h;
    }

    public int indexOf(int ch) {
        return indexOf(ch, 0);
    }

    public int indexOf(int ch, int fromIndex) {
        if (fromIndex < 0) {
            fromIndex = 0;
        }
        for (int i = fromIndex; i < count; i++) {
            if (value[offset + i] == ch) {
                return i;
            }
        }
        return -1;
    }

    public int lastIndexOf(int ch) {
        return lastIndexOf(ch, count - 1);
    }

    public int lastIndexOf(int ch, int fromIndex) {
        for (int i = Math.min(fromIndex, count - 1); i >= 0; i--) {
            if (value[offset + i] == ch) {
                return i;
            }
        }
        return -1;
    }

    public int indexOf(String str) {
        return indexOf(str, 0);
    }

    public int indexOf(String str, int fromIndex) {
        int n = str.count;
        if (n == 0) {
            return Math.min(Math.max(fromIndex, 0), count);
        }
        int last = count - n;
        for (int i = Math.max(fromIndex, 0); i <= last; i++) {
            if (regionMatches(false, i, str, 0, n)) {
                return i;
            }
        }
        return -1;
    }

    public int lastIndexOf(String str) {
        return lastIndexOf(str, count);
    }

    public int lastIndexOf(String str, int fromIndex) {
        int n = str.count;
        for (int i = Math.min(fromIndex, count - n); i >= 0; i--) {
            if (regionMatches(false, i, str, 0, n)) {
                return i;
            }
        }
        return -1;
    }

    public String substring(int beginIndex) {
        return substring(beginIndex, count);
    }

    /** The chars from beginIndex up to endIndex, sharing this string's array. */
    public String substring(int beginIndex, int endIndex) {
        if (beginIndex < 0 || endIndex > count || beginIndex > endIndex) {
            throw new StringIndexOutOfBoundsException("begin " + beginIndex + ", end " + endIndex + ", length " + count);
        }
        if (beginIndex == 0 && endIndex == count) {
            return this;
        }
        return new String(value, offset + beginIndex, endIndex - beginIndex, true);
    }

    public String concat(String str) {
        if (str.count == 0) {
            return this;
        }
        char[] chars = new char[count + str.count];
        System.arraycopy(value, offset, chars, 0, count);
        System.arraycopy(str.value, str.offset, chars, count, str.count);
        return new String(chars, true);
    }

    public String replace(char oldChar, char newChar) {
        int first = oldChar != newChar ? indexOf(oldChar) : -1;
        if (first < 0) {
            return this;
        }
        char[] chars = toCharArray();
        for (int i = first; i < count; i++) {
            if (chars[i] == oldChar) {
                chars[i] = newChar;
            }
        }
        return new String(chars, true);
    }

    public String toLowerCase() {
        return changeCase(false);
    }

    public String toUpperCase() {
        return changeCase(true);
    }

    private String changeCase(boolean upper) {
        char[] chars = null;
        for (int i = 0; i < count; i++) {
            char c = value[offset + i];
            char d = upper ? Character.toUpperCase(c) : Character.toLowerCase(c);
            if (d != c) {
                if (chars == null) {
                    chars = toCharArray();
                }
                chars[i] = d;
            }
        }
        return chars == null ? this : new String(chars, true);
    }

    /** Without the white space and control chars (up to ' ') at either end. */
    public String trim() {
        int begin = 0;
        int end = count;
        while (begin < end && value[offset + begin] <= ' ') {
            begin++;
        }
        while (end > begin && value[offset + end - 1] <= ' ') {
            end--;
        }
        return substring(begin, end);
    }

    public String toString() {
        return this;
    }

    /** The one string in the virtual machine's pool equal to this one. */
    public native String intern();

    public static String valueOf(Object obj) {
        return obj == null ? "null" : obj.toString();
    }

    public static String valueOf(char[] data) {
        return new String(data);
    }

    public static String valueOf(char[] data, int offset, int count) {
        return new String(data, offset, count);
    }

    public static String valueOf(boolean b) {
        return b ? "true" : "false";
    }

    public static String valueOf(char c) {
        return new String(new char[] {c}, true);
    }

    public static String valueOf(int i) {
        return Integer.toString(i);
    }

    public static String valueOf(long l) {
        return Long.toString(l);
    }

    public static String valueOf(float f) {
        return Float.toString(f);
    }

    public static String valueOf(double d) {
        return Double.toString(d);
    }
}
