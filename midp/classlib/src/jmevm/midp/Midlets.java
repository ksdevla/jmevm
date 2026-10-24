package jmevm.midp;

/**
 * Creates and starts the MIDlet. MIDlet calls back in through the Control it
 * installs when its class initializes, since its life-cycle methods are not
 * public; and only while Midlets constructs one may a MIDlet be made.
 */
public final class Midlets {
    /** The life cycle of a MIDlet, as the platform drives it. */
    public interface Control {
        /** Calls startApp(); reports and ends the program if it fails. */
        void start(Object midlet);
    }

    private static Control control;
    private static boolean constructing;

    private Midlets() {
    }

    public static synchronized void install(Control c) {
        if (control == null) {
            control = c;
        }
    }

    /** Whether a MIDlet may be constructed now (only by the platform). */
    public static synchronized boolean constructing() {
        return constructing;
    }

    private static synchronized void setConstructing(boolean b) {
        constructing = b;
    }

    /** The class of the suite's first MIDlet ("MIDlet-1: name, icon, class"). */
    private static String first() {
        String entry = AppProperties.get("MIDlet-1");
        if (entry == null) {
            return null;
        }
        int a = entry.indexOf(','), b = a < 0 ? -1 : entry.indexOf(',', a + 1);
        return b < 0 ? null : entry.substring(b + 1).trim();
    }

    static void run(String className) {
        if (className.length() == 0) {
            className = first();
        }
        if (className == null || className.length() == 0) {
            System.out.println("jmevm-midp: the manifest names no MIDlet (MIDlet-1)");
            Boot.exit(1);
            return;
        }
        Object midlet;
        try {
            Class c = Class.forName(className);
            setConstructing(true);
            midlet = c.newInstance();
        } catch (Throwable t) {
            System.out.println("jmevm-midp: cannot create the MIDlet " + className + ": " + t);
            Boot.exit(1);
            return;
        } finally {
            setConstructing(false);
        }
        control.start(midlet);
    }
}
