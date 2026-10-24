package java.util;

/** A task a Timer runs once or repeatedly. */
public abstract class TimerTask implements Runnable {
    static final int NEW = 0;
    static final int SCHEDULED = 1;
    static final int EXECUTED = 2;   /* a one-time task that ran */
    static final int CANCELLED = 3;

    /* Guarded by the timer's queue. */
    int state = NEW;
    long nextTime;   /* when to run next, in ms since 1970 */
    /** The repeat interval in ms: positive at a fixed rate, negative with a fixed delay, 0 for once. */
    long period;

    protected TimerTask() {
    }

    public abstract void run();

    /**
     * Stops the task from running again; true if that stopped a run that
     * was due (a scheduled one-time task, or any repeating task).
     */
    public boolean cancel() {
        synchronized (this) {
            boolean due = state == SCHEDULED;
            state = CANCELLED;
            return due;
        }
    }

    /** When the latest run was due to start. */
    public long scheduledExecutionTime() {
        synchronized (this) {
            return period < 0 ? nextTime + period : nextTime - period;
        }
    }
}
