package javax.microedition.midlet;

import javax.microedition.io.ConnectionNotFoundException;

import jmevm.midp.AppProperties;
import jmevm.midp.Boot;
import jmevm.midp.Midlets;

/**
 * An application. The platform makes one, starts it, and the program ends
 * when the MIDlet says it is done (notifyDestroyed). Nothing pauses it here.
 */
public abstract class MIDlet {
    static {
        Midlets.install(new Midlets.Control() {
            public void start(Object o) {
                MIDlet m = (MIDlet) o;
                try {
                    m.startApp();
                } catch (MIDletStateChangeException e) {
                    /* A transient failure: the MIDlet stays paused. */
                    System.out.println("jmevm-midp: the MIDlet did not start: " + e.getMessage());
                } catch (RuntimeException e) {
                    e.printStackTrace();
                    try {
                        m.destroyApp(true);
                    } catch (Throwable ignored) {
                    }
                    Boot.exit(1);
                }
            }
        });
    }

    protected MIDlet() {
        if (!Midlets.constructing()) {
            throw new SecurityException("only the platform creates MIDlets");
        }
    }

    protected abstract void startApp() throws MIDletStateChangeException;

    protected abstract void pauseApp();

    protected abstract void destroyApp(boolean unconditional) throws MIDletStateChangeException;

    /** The MIDlet has cleaned up and wants to end: the program ends. */
    public final void notifyDestroyed() {
        Boot.exit(0);
    }

    public final void notifyPaused() {
    }

    public final void resumeRequest() {
    }

    public final String getAppProperty(String key) {
        if (key == null) {
            throw new NullPointerException();
        }
        return AppProperties.get(key);
    }

    /** Nothing outside the MIDlet can be opened here: an empty URL cancels,
     *  any other cannot be handled. */
    public final boolean platformRequest(String url) throws ConnectionNotFoundException {
        if (url == null || url.length() == 0) {
            return false;
        }
        throw new ConnectionNotFoundException("cannot open " + url);
    }

    /** Every permission is granted. */
    public final int checkPermission(String permission) {
        return 1;
    }
}
