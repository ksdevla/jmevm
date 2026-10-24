package javax.microedition.io;

import java.io.DataInputStream;
import java.io.IOException;
import java.io.InputStream;

/** A connection that can be read. */
public interface InputConnection extends Connection {
    InputStream openInputStream() throws IOException;

    DataInputStream openDataInputStream() throws IOException;
}
