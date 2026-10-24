package javax.microedition.io;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

/**
 * Opens connections by name ("scheme:target"). The one kind there is reads
 * files of the application's jar: "resource:/path" (or "resource:path"). A
 * name without a scheme is taken as a path in the jar too, as some games
 * expect; when there is no such file it is an invalid name. Every other
 * scheme (http, socket, sms, ...) has no connection here: there is no
 * network.
 */
public class Connector {
    public static final int READ = 1;
    public static final int WRITE = 2;
    public static final int READ_WRITE = 3;

    private Connector() {
    }

    public static Connection open(String name) throws IOException {
        return open(name, READ_WRITE, false);
    }

    public static Connection open(String name, int mode) throws IOException {
        return open(name, mode, false);
    }

    public static Connection open(String name, int mode, boolean timeouts) throws IOException {
        if (name == null) {
            throw new IllegalArgumentException("no connection name");
        }
        if (mode != READ && mode != WRITE && mode != READ_WRITE) {
            throw new IllegalArgumentException("mode " + mode);
        }
        int colon = name.indexOf(':');
        int slash = name.indexOf('/');
        boolean schemed = colon > 0 && (slash < 0 || colon < slash);
        if (!schemed) {
            JarConnection c = JarConnection.open(name, mode);
            if (c == null) {
                throw new IllegalArgumentException("no scheme in " + name);
            }
            return c;
        }
        String scheme = name.substring(0, colon).toLowerCase();
        if (scheme.equals("resource")) {
            String path = name.substring(colon + 1);
            if (path.startsWith("//")) {
                path = path.substring(2);
            }
            JarConnection c = JarConnection.open(path, mode);
            if (c == null) {
                throw new ConnectionNotFoundException("no file " + path + " in the jar");
            }
            return c;
        }
        throw new ConnectionNotFoundException(scheme + ": connections are not available");
    }

    public static InputStream openInputStream(String name) throws IOException {
        return openDataInputStream(name);
    }

    public static DataInputStream openDataInputStream(String name) throws IOException {
        Connection c = open(name, READ);
        if (!(c instanceof InputConnection)) {
            c.close();
            throw new IllegalArgumentException(name + " cannot be read");
        }
        try {
            return ((InputConnection) c).openDataInputStream();
        } finally {
            c.close();   /* the stream stays usable */
        }
    }

    public static OutputStream openOutputStream(String name) throws IOException {
        return openDataOutputStream(name);
    }

    public static DataOutputStream openDataOutputStream(String name) throws IOException {
        Connection c = open(name, WRITE);
        if (!(c instanceof OutputConnection)) {
            c.close();
            throw new IllegalArgumentException(name + " cannot be written");
        }
        try {
            return ((OutputConnection) c).openDataOutputStream();
        } finally {
            c.close();
        }
    }
}
