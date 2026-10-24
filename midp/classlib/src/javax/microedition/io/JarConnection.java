package javax.microedition.io;

import java.io.ByteArrayInputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

import jmevm.midp.Resources;

/** A file of the application's jar, opened through Connector; read-only. */
final class JarConnection implements ContentConnection {
    private final byte[] data;
    private final int mode;
    private boolean closed;
    private boolean streamOpened;

    private JarConnection(byte[] data, int mode) {
        this.data = data;
        this.mode = mode;
    }

    /** The connection to a jar file, or null if there is no such file. */
    static JarConnection open(String path, int mode) {
        byte[] data = Resources.read(path.startsWith("/") ? path.substring(1) : path);
        return data == null ? null : new JarConnection(data, mode);
    }

    private void check() throws IOException {
        if (closed) {
            throw new IOException("the connection is closed");
        }
    }

    public InputStream openInputStream() throws IOException {
        return openDataInputStream();
    }

    public DataInputStream openDataInputStream() throws IOException {
        check();
        if (mode == Connector.WRITE) {
            throw new IOException("opened for writing only");
        }
        if (streamOpened) {
            throw new IOException("the stream was opened already");
        }
        streamOpened = true;
        return new DataInputStream(new ByteArrayInputStream(data));
    }

    public OutputStream openOutputStream() throws IOException {
        return openDataOutputStream();
    }

    public DataOutputStream openDataOutputStream() throws IOException {
        throw new IOException("jar files cannot be written");
    }

    public String getType() {
        return null;
    }

    public String getEncoding() {
        return null;
    }

    public long getLength() {
        return data.length;
    }

    public void close() {
        closed = true;
    }
}
