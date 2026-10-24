package javax.microedition.media;

import java.util.Vector;

/**
 * The one thread behind all players: it delivers their events to the
 * listeners in the order they were posted, and tells each started player
 * when its media should have reached the end. It starts with the first
 * player and then waits for work.
 */
final class MediaThread extends Thread {
    private static MediaThread instance;

    private final Vector events = new Vector();   /* {player, event, data} */
    private final Vector timed = new Vector();    /* players that may have an end time */
    private boolean changed;                      /* something was posted since the last look */

    private MediaThread() {
        super("media");
    }

    private static synchronized MediaThread get() {
        if (instance == null) {
            instance = new MediaThread();
            instance.start();
        }
        return instance;
    }

    static void post(AudioPlayer player, String event, Object data) {
        MediaThread t = get();
        synchronized (t) {
            t.events.addElement(new Object[] {player, event, data});
            t.changed = true;
            t.notifyAll();
        }
    }

    /** The player wants end() at its endAt time (or no longer, if it stopped). */
    static void timeChanged(AudioPlayer player) {
        MediaThread t = get();
        synchronized (t) {
            if (!t.timed.contains(player)) {
                t.timed.addElement(player);
            }
            t.changed = true;
            t.notifyAll();
        }
    }

    /*
     * Players lock themselves and then post here; so this thread never calls
     * a player while it holds its own lock. What changed between looking at
     * the players and waiting sets `changed`, and the wait is skipped.
     */
    public void run() {
        for (;;) {
            AudioPlayer[] players;
            synchronized (this) {
                players = new AudioPlayer[timed.size()];
                timed.copyInto(players);
                changed = false;
            }
            long now = System.currentTimeMillis();
            long wake = Long.MAX_VALUE;
            AudioPlayer due = null;
            for (int i = 0; i < players.length; i++) {
                long at = players[i].endAt();
                if (at < 0) {
                    synchronized (this) {
                        if (!changed) {   /* not restarted meanwhile */
                            timed.removeElement(players[i]);
                        }
                    }
                } else if (at <= now) {
                    due = players[i];
                } else if (at < wake) {
                    wake = at;
                }
            }
            if (due != null) {
                due.end();
                continue;
            }
            Object[] event = null;
            synchronized (this) {
                if (events.size() > 0) {
                    event = (Object[]) events.elementAt(0);
                    events.removeElementAt(0);
                } else if (!changed) {
                    try {
                        wait(wake == Long.MAX_VALUE ? 0 : wake - now);
                    } catch (InterruptedException e) {
                        return;
                    }
                }
            }
            if (event != null) {
                ((AudioPlayer) event[0]).deliver((String) event[1], event[2]);
            }
        }
    }
}
