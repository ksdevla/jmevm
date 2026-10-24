import java.util.Timer;
import java.util.TimerTask;
import java.util.Vector;

/*
 * Threads: start and join, monitors, wait and notify, interruption, sleep,
 * priorities and java.util.Timer. Every line printed is the same however the
 * threads interleave, so the expected output is what a desktop JVM prints.
 */
public class TestConcurrency {
    static int counter;
    static final Object LOCK = new Object();

    static class Counter extends Thread {
        final int times;

        Counter(String name, int times) {
            super(name);
            this.times = times;
        }

        public void run() {
            for (int i = 0; i < times; i++) {
                synchronized (LOCK) {
                    counter++;
                }
                if (i % 100 == 0) {
                    Thread.yield();
                }
            }
        }
    }

    /** A bounded queue: put() waits while full, take() while empty. */
    static class Queue {
        private final int[] items = new int[4];
        private int head;
        private int size;

        synchronized void put(int v) throws InterruptedException {
            while (size == items.length) {
                wait();
            }
            items[(head + size) % items.length] = v;
            size++;
            notifyAll();
        }

        synchronized int take() throws InterruptedException {
            while (size == 0) {
                wait();
            }
            int v = items[head];
            head = (head + 1) % items.length;
            size--;
            notifyAll();
            return v;
        }
    }

    public static void main(String[] args) throws Exception {
        startAndJoin();
        producerConsumer();
        interruption();
        sleeping();
        timers();
        System.out.println("done");
    }

    static void startAndJoin() throws InterruptedException {
        Thread main = Thread.currentThread();
        System.out.println("main " + main.getName() + " " + main.getPriority() + " " + main.isAlive());
        Counter[] counters = new Counter[4];
        for (int i = 0; i < counters.length; i++) {
            counters[i] = new Counter("counter-" + i, 1000 * (i + 1));
        }
        System.out.println("before start " + counters[0].isAlive() + " " + counters[3].getName());
        for (int i = 0; i < counters.length; i++) {
            counters[i].start();
        }
        for (int i = 0; i < counters.length; i++) {
            counters[i].join();
        }
        System.out.println("counted " + counter + " alive " + counters[0].isAlive());
        try {
            counters[0].start();
        } catch (IllegalThreadStateException e) {
            System.out.println("started twice");
        }
        final StringBuffer ran = new StringBuffer();
        Thread r = new Thread(new Runnable() {
            public void run() {
                ran.append("runnable ran in ").append(Thread.currentThread().getName().startsWith("Thread-"));
            }
        });
        r.setPriority(Thread.MAX_PRIORITY);
        System.out.println("priority " + r.getPriority() + " inherited " + new Thread().getPriority());
        r.start();
        r.join();
        System.out.println(ran);
        try {
            r.setPriority(11);
        } catch (IllegalArgumentException e) {
            System.out.println("bad priority");
        }
        System.out.println("toString names it " + (new Thread("named").toString().indexOf("named") > 0));
    }

    static void producerConsumer() throws InterruptedException {
        final Queue q = new Queue();
        final int[] sum = new int[1];
        Thread consumer = new Thread() {
            public void run() {
                try {
                    for (int v; (v = q.take()) >= 0;) {
                        sum[0] += v;
                    }
                } catch (InterruptedException e) {
                    sum[0] = -1;
                }
            }
        };
        consumer.start();
        for (int i = 1; i <= 500; i++) {
            q.put(i);
        }
        q.put(-1);
        consumer.join();
        System.out.println("consumed " + sum[0]);
    }

    static void interruption() throws InterruptedException {
        final Vector events = new Vector();
        Thread sleeper = new Thread() {
            public void run() {
                try {
                    Thread.sleep(100000);
                    events.addElement("slept");
                } catch (InterruptedException e) {
                    events.addElement("sleep interrupted");
                }
                synchronized (this) {
                    try {
                        wait();
                        events.addElement("woke");
                    } catch (InterruptedException e) {
                        events.addElement("wait interrupted");
                    }
                }
            }
        };
        sleeper.start();
        while (events.size() < 1) {
            sleeper.interrupt();
            Thread.sleep(5);
        }
        while (sleeper.isAlive()) {
            sleeper.interrupt();
            Thread.sleep(5);
        }
        System.out.println("events " + events);
        Thread.currentThread().interrupt();
        try {
            Thread.sleep(10);
            System.out.println("not interrupted");
        } catch (InterruptedException e) {
            System.out.println("pending interrupt");
        }
        try {
            LOCK.wait();
        } catch (IllegalMonitorStateException e) {
            System.out.println("wait without the lock");
        }
        try {
            LOCK.notify();
        } catch (IllegalMonitorStateException e) {
            System.out.println("notify without the lock");
        }
        try {
            Thread.sleep(-1);
        } catch (IllegalArgumentException e) {
            System.out.println("negative sleep");
        }
        synchronized (LOCK) {
            long t = System.currentTimeMillis();
            LOCK.wait(30);
            System.out.println("timed wait " + (System.currentTimeMillis() - t >= 25));
            synchronized (LOCK) {
                System.out.println("reentrant");
            }
        }
    }

    static void sleeping() throws InterruptedException {
        final Vector order = new Vector();
        Thread[] sleepers = new Thread[3];
        int[] delays = {60, 20, 40};
        for (int i = 0; i < sleepers.length; i++) {
            final int delay = delays[i];
            sleepers[i] = new Thread() {
                public void run() {
                    try {
                        Thread.sleep(delay);
                    } catch (InterruptedException e) {
                        return;
                    }
                    order.addElement(new Integer(delay));
                }
            };
            sleepers[i].start();
        }
        for (int i = 0; i < sleepers.length; i++) {
            sleepers[i].join();
        }
        System.out.println("woke in order " + order);
    }

    static void timers() throws InterruptedException {
        final Vector log = new Vector();
        final Timer timer = new Timer();
        TimerTask once = new TimerTask() {
            public void run() {
                log.addElement("once");
            }
        };
        TimerTask repeat = new TimerTask() {
            int runs;

            public void run() {
                log.addElement("repeat " + ++runs);
                if (runs == 3) {
                    cancel();
                    synchronized (log) {
                        log.notify();
                    }
                }
            }
        };
        TimerTask never = new TimerTask() {
            public void run() {
                log.addElement("never");
            }
        };
        timer.schedule(once, 10);
        timer.scheduleAtFixedRate(repeat, 100, 50);
        timer.schedule(never, 10000);
        System.out.println("cancel unrun " + never.cancel() + " " + never.cancel());
        synchronized (log) {
            while (log.size() < 4) {
                log.wait();
            }
        }
        System.out.println("timer " + log);
        try {
            timer.schedule(once, 10);
        } catch (IllegalStateException e) {
            System.out.println("task reused");
        }
        try {
            timer.schedule(new TimerTask() {
                public void run() {
                }
            }, -1);
        } catch (IllegalArgumentException e) {
            System.out.println("negative delay");
        }
        timer.cancel();
        try {
            timer.schedule(new TimerTask() {
                public void run() {
                }
            }, 1);
        } catch (IllegalStateException e) {
            System.out.println("timer cancelled");
        }
    }
}
