package java.util;

/** A last-in, first-out stack of objects: a vector whose end is the top. */
public class Stack extends Vector {
    public Stack() {
    }

    public Object push(Object item) {
        addElement(item);
        return item;
    }

    public synchronized Object pop() {
        Object top = peek();
        removeElementAt(elementCount - 1);
        return top;
    }

    public synchronized Object peek() {
        if (elementCount == 0) {
            throw new EmptyStackException();
        }
        return elementData[elementCount - 1];
    }

    public boolean empty() {
        return elementCount == 0;
    }

    /** How far o is from the top (the top is 1), or -1 if it is not on the stack. */
    public synchronized int search(Object o) {
        int i = lastIndexOf(o);
        return i >= 0 ? elementCount - i : -1;
    }
}
