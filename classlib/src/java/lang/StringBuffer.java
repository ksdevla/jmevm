package java.lang;

/**
 * A growable sequence of chars that threads can share: a StringBuilder
 * whose every operation holds this buffer's lock.
 */
public final class StringBuffer {
    private final StringBuilder chars;

    public StringBuffer() {
        chars = new StringBuilder();
    }

    public StringBuffer(int capacity) {
        chars = new StringBuilder(capacity);
    }

    public StringBuffer(String str) {
        chars = new StringBuilder(str);
    }

    public synchronized int length() {
        return chars.length();
    }

    public synchronized int capacity() {
        return chars.capacity();
    }

    public synchronized void ensureCapacity(int minimumCapacity) {
        chars.ensureCapacity(minimumCapacity);
    }

    public synchronized void setLength(int newLength) {
        chars.setLength(newLength);
    }

    public synchronized char charAt(int index) {
        return chars.charAt(index);
    }

    public synchronized void getChars(int srcBegin, int srcEnd, char[] dst, int dstBegin) {
        chars.getChars(srcBegin, srcEnd, dst, dstBegin);
    }

    public synchronized void setCharAt(int index, char ch) {
        chars.setCharAt(index, ch);
    }

    public StringBuffer append(Object obj) {
        return append(String.valueOf(obj));
    }

    public synchronized StringBuffer append(String str) {
        chars.append(str);
        return this;
    }

    public synchronized StringBuffer append(char[] str) {
        chars.append(str);
        return this;
    }

    public synchronized StringBuffer append(char[] str, int offset, int len) {
        chars.append(str, offset, len);
        return this;
    }

    public StringBuffer append(boolean b) {
        return append(b ? "true" : "false");
    }

    public synchronized StringBuffer append(char c) {
        chars.append(c);
        return this;
    }

    public StringBuffer append(int i) {
        return append(Integer.toString(i));
    }

    public StringBuffer append(long l) {
        return append(Long.toString(l));
    }

    public StringBuffer append(float f) {
        return append(Float.toString(f));
    }

    public StringBuffer append(double d) {
        return append(Double.toString(d));
    }

    public synchronized StringBuffer delete(int start, int end) {
        chars.delete(start, end);
        return this;
    }

    public synchronized StringBuffer deleteCharAt(int index) {
        chars.deleteCharAt(index);
        return this;
    }

    public StringBuffer insert(int offset, Object obj) {
        return insert(offset, String.valueOf(obj));
    }

    public synchronized StringBuffer insert(int offset, String str) {
        chars.insert(offset, str);
        return this;
    }

    public synchronized StringBuffer insert(int offset, char[] str) {
        chars.insert(offset, str);
        return this;
    }

    public StringBuffer insert(int offset, boolean b) {
        return insert(offset, b ? "true" : "false");
    }

    public synchronized StringBuffer insert(int offset, char c) {
        chars.insert(offset, c);
        return this;
    }

    public StringBuffer insert(int offset, int i) {
        return insert(offset, Integer.toString(i));
    }

    public StringBuffer insert(int offset, long l) {
        return insert(offset, Long.toString(l));
    }

    public StringBuffer insert(int offset, float f) {
        return insert(offset, Float.toString(f));
    }

    public StringBuffer insert(int offset, double d) {
        return insert(offset, Double.toString(d));
    }

    public synchronized StringBuffer reverse() {
        chars.reverse();
        return this;
    }

    public synchronized String toString() {
        return chars.toString();
    }
}
