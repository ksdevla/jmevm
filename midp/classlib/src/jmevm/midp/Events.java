package jmevm.midp;

/**
 * Input from the platform (natives in src/midp_jme/events.c): one queue,
 * read by the UI thread.
 */
public final class Events {
    public static final int KEY_DOWN = 1;
    public static final int KEY_UP = 2;
    public static final int POINTER_DOWN = 3;
    public static final int POINTER_UP = 4;
    public static final int POINTER_DRAG = 5;

    private Events() {
    }

    /**
     * Waits for the next event: its type, with the key code (or x and y) in
     * out[0] (and out[1]). 0 after timeoutMs (negative: no timeout) or after
     * wakeup().
     */
    public static native int next(int[] out, int timeoutMs);

    /** Makes a waiting next() (or the next one) return 0 at once. */
    public static native void wakeup();
}
