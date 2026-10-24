package java.lang;

/**
 * A thread of execution. The virtual machine schedules threads itself; it
 * makes the main thread's object without a constructor, filling in name and
 * priority, which it reads by name.
 */
public class Thread implements Runnable {
    public static final int MIN_PRIORITY = 1;
    public static final int NORM_PRIORITY = 5;
    public static final int MAX_PRIORITY = 10;

    private static int created;   /* numbers the threads named by default */

    private String name;
    private int priority;
    private Runnable target;
    private boolean started;

    public Thread() {
        this(null, null);
    }

    public Thread(Runnable target) {
        this(target, null);
    }

    public Thread(String name) {
        this(null, name);
    }

    public Thread(Runnable target, String name) {
        this.target = target;
        this.name = name != null ? name : "Thread-" + nextNumber();
        Thread creator = currentThread();
        priority = creator != null ? creator.priority : NORM_PRIORITY;
    }

    private static synchronized int nextNumber() {
        return created++;
    }

    public static native Thread currentThread();

    /** Lets other threads of the same priority run. */
    public static native void yield();

    public static native void sleep(long millis) throws InterruptedException;

    /** Starts the thread, which calls run(); a thread starts only once. */
    public synchronized void start() {
        if (started || isAlive()) {
            throw new IllegalThreadStateException();
        }
        started = true;
        start0();
    }

    private native void start0();

    /** Runs the target given to the constructor; subclasses override it. */
    public void run() {
        if (target != null) {
            target.run();
        }
    }

    public final native boolean isAlive();

    /** Waits for the thread to end. */
    public final native void join() throws InterruptedException;

    /** Wakes the thread from sleep() or wait() with an InterruptedException. */
    public void interrupt() {
        interrupt0();
    }

    private native void interrupt0();

    /** The number of threads that started and have not ended. */
    public static native int activeCount();

    public final void setPriority(int newPriority) {
        if (newPriority < MIN_PRIORITY || newPriority > MAX_PRIORITY) {
            throw new IllegalArgumentException();
        }
        priority = newPriority;
        setPriority0(newPriority);
    }

    private native void setPriority0(int newPriority);

    public final int getPriority() {
        return priority;
    }

    public final String getName() {
        return name;
    }

    public String toString() {
        return "Thread[" + name + "," + priority + "]";
    }
}
