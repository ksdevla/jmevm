package java.util;

/**
 * Runs tasks on a thread of its own, at a time or repeatedly. Tasks run one
 * at a time, in the order they fall due; a slow task delays the others.
 * A task that throws ends the timer, as on Java SE.
 */
public class Timer {
    private static int created;   /* numbers the timers' threads */

    private final Vector queue = new Vector();   /* scheduled tasks, by nextTime, then by scheduling order */
    private boolean cancelled;

    public Timer() {
        new Thread(new Runnable() {
            public void run() {
                loop();
            }
        }, "Timer-" + nextNumber()).start();
    }

    private static synchronized int nextNumber() {
        return created++;
    }

    /** Runs task once, delay ms from now. */
    public void schedule(TimerTask task, long delay) {
        if (delay < 0) {
            throw new IllegalArgumentException("Negative delay.");
        }
        add(task, System.currentTimeMillis() + delay, 0);
    }

    /** Runs task once, at time (or now, if that has passed). */
    public void schedule(TimerTask task, Date time) {
        add(task, time.getTime(), 0);
    }

    /** Runs task from delay ms from now on, period ms after each run started late or not. */
    public void schedule(TimerTask task, long delay, long period) {
        if (delay < 0) {
            throw new IllegalArgumentException("Negative delay.");
        }
        add(task, System.currentTimeMillis() + delay, -checkPeriod(period));
    }

    public void schedule(TimerTask task, Date firstTime, long period) {
        add(task, firstTime.getTime(), -checkPeriod(period));
    }

    /** Runs task from delay ms from now on, every period ms, catching up when late. */
    public void scheduleAtFixedRate(TimerTask task, long delay, long period) {
        if (delay < 0) {
            throw new IllegalArgumentException("Negative delay.");
        }
        add(task, System.currentTimeMillis() + delay, checkPeriod(period));
    }

    public void scheduleAtFixedRate(TimerTask task, Date firstTime, long period) {
        add(task, firstTime.getTime(), checkPeriod(period));
    }

    private static long checkPeriod(long period) {
        if (period <= 0) {
            throw new IllegalArgumentException("Non-positive period.");
        }
        return period;
    }

    private void add(TimerTask task, long time, long period) {
        if (time < 0) {
            throw new IllegalArgumentException("Illegal execution time.");
        }
        synchronized (queue) {
            if (cancelled) {
                throw new IllegalStateException("Timer already cancelled.");
            }
            synchronized (task) {
                if (task.state != TimerTask.NEW) {
                    throw new IllegalStateException("Task already scheduled or cancelled");
                }
                task.state = TimerTask.SCHEDULED;
                task.nextTime = time;
                task.period = period;
            }
            insert(task);
            queue.notify();
        }
    }

    /** Puts task in the queue after every task due no later than it. */
    private void insert(TimerTask task) {
        int i = queue.size();
        while (i > 0 && ((TimerTask) queue.elementAt(i - 1)).nextTime > task.nextTime) {
            i--;
        }
        queue.insertElementAt(task, i);
    }

    /** Ends the timer: no task runs any more (but one that is running finishes). */
    public void cancel() {
        synchronized (queue) {
            cancelled = true;
            queue.removeAllElements();
            queue.notify();
        }
    }

    private void loop() {
        boolean finished = false;
        try {
            while (true) {
                TimerTask task;
                synchronized (queue) {
                    while (queue.isEmpty() && !cancelled) {
                        queue.wait();
                    }
                    if (cancelled) {
                        break;
                    }
                    task = (TimerTask) queue.elementAt(0);
                    long now = System.currentTimeMillis();
                    boolean due;
                    synchronized (task) {
                        if (task.state == TimerTask.CANCELLED) {
                            queue.removeElementAt(0);
                            continue;
                        }
                        due = task.nextTime <= now;
                        if (due) {
                            queue.removeElementAt(0);
                            if (task.period == 0) {
                                task.state = TimerTask.EXECUTED;
                            } else {
                                task.nextTime = task.period < 0 ? now - task.period : task.nextTime + task.period;
                                insert(task);
                            }
                        }
                    }
                    if (!due) {
                        queue.wait(task.nextTime - now);
                        continue;
                    }
                }
                task.run();
            }
            finished = true;
        } catch (InterruptedException e) {
            finished = true;
        } finally {
            if (!finished) {
                cancel();   /* a task threw: the timer is dead */
            }
        }
    }
}
