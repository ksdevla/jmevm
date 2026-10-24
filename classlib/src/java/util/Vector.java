package java.util;

/** A growable array of objects, safe to share between threads. */
public class Vector {
    protected Object[] elementData;
    protected int elementCount;
    /** How much the array grows when full; 0 or less doubles it. */
    protected int capacityIncrement;

    public Vector(int initialCapacity, int capacityIncrement) {
        if (initialCapacity < 0) {
            throw new IllegalArgumentException("Illegal Capacity: " + initialCapacity);
        }
        elementData = new Object[initialCapacity];
        this.capacityIncrement = capacityIncrement;
    }

    public Vector(int initialCapacity) {
        this(initialCapacity, 0);
    }

    public Vector() {
        this(10, 0);
    }

    public synchronized void copyInto(Object[] anArray) {
        System.arraycopy(elementData, 0, anArray, 0, elementCount);
    }

    public synchronized void trimToSize() {
        if (elementData.length > elementCount) {
            Object[] exact = new Object[elementCount];
            System.arraycopy(elementData, 0, exact, 0, elementCount);
            elementData = exact;
        }
    }

    public synchronized void ensureCapacity(int minCapacity) {
        int capacity = elementData.length;
        if (minCapacity > capacity) {
            int grown = capacityIncrement > 0 ? capacity + capacityIncrement : capacity * 2;
            Object[] bigger = new Object[Math.max(grown, minCapacity)];
            System.arraycopy(elementData, 0, bigger, 0, elementCount);
            elementData = bigger;
        }
    }

    /** Cuts the vector to newSize elements, or pads it with nulls. */
    public synchronized void setSize(int newSize) {
        if (newSize < 0) {
            throw new ArrayIndexOutOfBoundsException(newSize);
        }
        ensureCapacity(newSize);
        for (int i = newSize; i < elementCount; i++) {
            elementData[i] = null;
        }
        elementCount = newSize;
    }

    public int capacity() {
        return elementData.length;
    }

    public int size() {
        return elementCount;
    }

    public boolean isEmpty() {
        return elementCount == 0;
    }

    /** The elements in order, as the vector holds them when each is fetched. */
    public Enumeration elements() {
        return new Enumeration() {
            private int next;

            public boolean hasMoreElements() {
                return next < elementCount;
            }

            public Object nextElement() {
                synchronized (Vector.this) {
                    if (next < elementCount) {
                        return elementData[next++];
                    }
                }
                throw new NoSuchElementException("Vector Enumeration");
            }
        };
    }

    public boolean contains(Object elem) {
        return indexOf(elem, 0) >= 0;
    }

    public int indexOf(Object elem) {
        return indexOf(elem, 0);
    }

    /** The first index from index on that holds an element equal to elem, or -1. */
    public synchronized int indexOf(Object elem, int index) {
        for (int i = index; i < elementCount; i++) {
            if (elem == null ? elementData[i] == null : elem.equals(elementData[i])) {
                return i;
            }
        }
        return -1;
    }

    public synchronized int lastIndexOf(Object elem) {
        return lastIndexOf(elem, elementCount - 1);
    }

    /** The last index up to index that holds an element equal to elem, or -1. */
    public synchronized int lastIndexOf(Object elem, int index) {
        if (index >= elementCount) {
            throw new IndexOutOfBoundsException(index + " >= " + elementCount);
        }
        for (int i = index; i >= 0; i--) {
            if (elem == null ? elementData[i] == null : elem.equals(elementData[i])) {
                return i;
            }
        }
        return -1;
    }

    private void checkIndex(int index) {
        if (index >= elementCount) {
            throw new ArrayIndexOutOfBoundsException(index + " >= " + elementCount);
        }
        if (index < 0) {
            throw new ArrayIndexOutOfBoundsException(index);
        }
    }

    public synchronized Object elementAt(int index) {
        checkIndex(index);
        return elementData[index];
    }

    public synchronized Object firstElement() {
        if (elementCount == 0) {
            throw new NoSuchElementException();
        }
        return elementData[0];
    }

    public synchronized Object lastElement() {
        if (elementCount == 0) {
            throw new NoSuchElementException();
        }
        return elementData[elementCount - 1];
    }

    public synchronized void setElementAt(Object obj, int index) {
        checkIndex(index);
        elementData[index] = obj;
    }

    public synchronized void removeElementAt(int index) {
        checkIndex(index);
        System.arraycopy(elementData, index + 1, elementData, index, elementCount - index - 1);
        elementData[--elementCount] = null;
    }

    public synchronized void insertElementAt(Object obj, int index) {
        if (index > elementCount) {
            throw new ArrayIndexOutOfBoundsException(index + " > " + elementCount);
        }
        if (index < 0) {
            throw new ArrayIndexOutOfBoundsException(index);
        }
        ensureCapacity(elementCount + 1);
        System.arraycopy(elementData, index, elementData, index + 1, elementCount - index);
        elementData[index] = obj;
        elementCount++;
    }

    public synchronized void addElement(Object obj) {
        ensureCapacity(elementCount + 1);
        elementData[elementCount++] = obj;
    }

    /** Removes the first element equal to obj; false if there is none. */
    public synchronized boolean removeElement(Object obj) {
        int i = indexOf(obj, 0);
        if (i < 0) {
            return false;
        }
        removeElementAt(i);
        return true;
    }

    public synchronized void removeAllElements() {
        for (int i = 0; i < elementCount; i++) {
            elementData[i] = null;
        }
        elementCount = 0;
    }

    /** "[a, b, c]". */
    public synchronized String toString() {
        StringBuffer sb = new StringBuffer("[");
        for (int i = 0; i < elementCount; i++) {
            if (i > 0) {
                sb.append(", ");
            }
            sb.append(elementData[i] == this ? "(this Collection)" : String.valueOf(elementData[i]));
        }
        return sb.append(']').toString();
    }
}
