package java.util;

/** Hands out a series of elements, one at a time. */
public interface Enumeration {
    boolean hasMoreElements();

    /** The next element; NoSuchElementException if there is none. */
    Object nextElement();
}
