package java.lang;

/**
 * A growable sequence of chars, for one thread. Not in CLDC 1.1, but javac
 * compiles string concatenation (+) to it, and StringBuffer is built on it.
 */
public final class StringBuilder {
    private char[] value;
    private int count;

    public StringBuilder() {
        this(16);
    }

    public StringBuilder(int capacity) {
        value = new char[capacity];
    }

    public StringBuilder(String str) {
        this(str.length() + 16);
        append(str);
    }

    public int length() {
        return count;
    }

    public int capacity() {
        return value.length;
    }

    public void ensureCapacity(int minimumCapacity) {
        if (minimumCapacity > value.length) {
            int capacity = Math.max(minimumCapacity, value.length * 2 + 2);
            char[] bigger = new char[capacity];
            System.arraycopy(value, 0, bigger, 0, count);
            value = bigger;
        }
    }

    /** Cuts the sequence, or pads it with '\u0000' chars, to newLength. */
    public void setLength(int newLength) {
        if (newLength < 0) {
            throw new StringIndexOutOfBoundsException(newLength);
        }
        ensureCapacity(newLength);
        for (int i = count; i < newLength; i++) {
            value[i] = 0;
        }
        count = newLength;
    }

    public char charAt(int index) {
        checkIndex(index);
        return value[index];
    }

    public void getChars(int srcBegin, int srcEnd, char[] dst, int dstBegin) {
        if (srcBegin < 0 || srcEnd > count || srcBegin > srcEnd) {
            throw new StringIndexOutOfBoundsException("start " + srcBegin + ", end " + srcEnd + ", length " + count);
        }
        System.arraycopy(value, srcBegin, dst, dstBegin, srcEnd - srcBegin);
    }

    public void setCharAt(int index, char ch) {
        checkIndex(index);
        value[index] = ch;
    }

    private void checkIndex(int index) {
        if (index < 0 || index >= count) {
            throw new StringIndexOutOfBoundsException(index);
        }
    }

    public StringBuilder append(Object obj) {
        return append(String.valueOf(obj));
    }

    public StringBuilder append(String str) {
        if (str == null) {
            str = "null";
        }
        int n = str.length();
        ensureCapacity(count + n);
        str.getChars(0, n, value, count);
        count += n;
        return this;
    }

    public StringBuilder append(StringBuffer sb) {
        return append(sb == null ? "null" : sb.toString());
    }

    public StringBuilder append(char[] str) {
        return append(str, 0, str.length);
    }

    public StringBuilder append(char[] str, int offset, int len) {
        if (offset < 0 || len < 0 || len > str.length - offset) {
            throw new IndexOutOfBoundsException("offset " + offset + ", count " + len + ", length " + str.length);
        }
        ensureCapacity(count + len);
        System.arraycopy(str, offset, value, count, len);
        count += len;
        return this;
    }

    public StringBuilder append(boolean b) {
        return append(b ? "true" : "false");
    }

    public StringBuilder append(char c) {
        ensureCapacity(count + 1);
        value[count++] = c;
        return this;
    }

    public StringBuilder append(int i) {
        return append(Integer.toString(i));
    }

    public StringBuilder append(long l) {
        return append(Long.toString(l));
    }

    public StringBuilder append(float f) {
        return append(Float.toString(f));
    }

    public StringBuilder append(double d) {
        return append(Double.toString(d));
    }

    /** Removes the chars from start up to end (or the end of the sequence). */
    public StringBuilder delete(int start, int end) {
        if (end > count) {
            end = count;
        }
        if (start < 0 || start > end) {
            throw new StringIndexOutOfBoundsException("start " + start + ", end " + end + ", length " + count);
        }
        System.arraycopy(value, end, value, start, count - end);
        count -= end - start;
        return this;
    }

    public StringBuilder deleteCharAt(int index) {
        checkIndex(index);
        return delete(index, index + 1);
    }

    public StringBuilder insert(int offset, Object obj) {
        return insert(offset, String.valueOf(obj));
    }

    public StringBuilder insert(int offset, String str) {
        if (str == null) {
            str = "null";
        }
        int n = str.length();
        openGap(offset, n);
        str.getChars(0, n, value, offset);
        return this;
    }

    public StringBuilder insert(int offset, char[] str) {
        openGap(offset, str.length);
        System.arraycopy(str, 0, value, offset, str.length);
        return this;
    }

    public StringBuilder insert(int offset, boolean b) {
        return insert(offset, b ? "true" : "false");
    }

    public StringBuilder insert(int offset, char c) {
        openGap(offset, 1);
        value[offset] = c;
        return this;
    }

    public StringBuilder insert(int offset, int i) {
        return insert(offset, Integer.toString(i));
    }

    public StringBuilder insert(int offset, long l) {
        return insert(offset, Long.toString(l));
    }

    public StringBuilder insert(int offset, float f) {
        return insert(offset, Float.toString(f));
    }

    public StringBuilder insert(int offset, double d) {
        return insert(offset, Double.toString(d));
    }

    /** Makes room for n chars at offset, moving the rest along. */
    private void openGap(int offset, int n) {
        if (offset < 0 || offset > count) {
            throw new StringIndexOutOfBoundsException("offset " + offset + ", length " + count);
        }
        ensureCapacity(count + n);
        System.arraycopy(value, offset, value, offset + n, count - offset);
        count += n;
    }

    /** Reverses the chars, keeping each surrogate pair in its order. */
    public StringBuilder reverse() {
        boolean surrogates = false;
        for (int i = 0, j = count - 1; i <= j; i++, j--) {
            char a = value[i];
            char b = value[j];
            surrogates |= (a >= '\ud800' && a <= '\udfff') || (b >= '\ud800' && b <= '\udfff');
            value[i] = b;
            value[j] = a;
        }
        if (surrogates) {
            for (int i = 0; i < count - 1; i++) {
                char low = value[i];
                char high = value[i + 1];
                if (low >= '\udc00' && low <= '\udfff' && high >= '\ud800' && high <= '\udbff') {
                    value[i] = high;
                    value[i + 1] = low;
                    i++;
                }
            }
        }
        return this;
    }

    public String toString() {
        return new String(value, 0, count);
    }
}
