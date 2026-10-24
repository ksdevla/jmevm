package javax.microedition.io;

import java.io.IOException;

/**
 * The push registry. Nothing can start a MIDlet from outside here (no
 * network, no messages, no timers while it is not running), so nothing is
 * registered: registrations fail and lookups find nothing.
 */
public class PushRegistry {
    private PushRegistry() {
    }

    public static void registerConnection(String connection, String midlet, String filter)
            throws ClassNotFoundException, IOException {
        if (connection == null || midlet == null || filter == null) {
            throw new IllegalArgumentException("connection, MIDlet and filter are needed");
        }
        throw new ConnectionNotFoundException("push connections are not available");
    }

    public static boolean unregisterConnection(String connection) {
        return false;
    }

    public static String[] listConnections(boolean available) {
        return new String[0];
    }

    public static String getMIDlet(String connection) {
        return null;
    }

    public static String getFilter(String connection) {
        return null;
    }

    public static long registerAlarm(String midlet, long time) throws ClassNotFoundException,
            ConnectionNotFoundException {
        if (midlet == null) {
            throw new IllegalArgumentException("no MIDlet");
        }
        throw new ConnectionNotFoundException("alarms are not available");
    }
}
