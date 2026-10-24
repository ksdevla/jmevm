package java.lang;

/**
 * The root of the class hierarchy. Identity, hashing, cloning and monitors
 * are the virtual machine's natives.
 */
public class Object {
    public Object() {
    }

    public final native Class getClass();

    public native int hashCode();

    public boolean equals(Object obj) {
        return this == obj;
    }

    /** Copies an array; any other object throws CloneNotSupportedException. */
    protected native Object clone() throws CloneNotSupportedException;

    public String toString() {
        return getClass().getName() + '@' + Integer.toHexString(hashCode());
    }

    public final native void notify();

    public final native void notifyAll();

    /** Waits until notified or interrupted, or for timeout ms (0: no limit). */
    public final native void wait(long timeout) throws InterruptedException;

    /** Waits as wait(long); a non-zero nanos rounds the timeout up a millisecond. */
    public final void wait(long timeout, int nanos) throws InterruptedException {
        if (timeout < 0) {
            throw new IllegalArgumentException("timeout value is negative");
        }
        if (nanos < 0 || nanos > 999999) {
            throw new IllegalArgumentException("nanosecond timeout value out of range");
        }
        if (nanos > 0 && timeout != Long.MAX_VALUE) {
            timeout++;
        }
        wait(timeout);
    }

    public final void wait() throws InterruptedException {
        wait(0);
    }
}
