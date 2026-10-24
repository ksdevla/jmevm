package javax.microedition.io;

import java.io.IOException;

/** Waits for incoming stream connections. */
public interface StreamConnectionNotifier extends Connection {
    StreamConnection acceptAndOpen() throws IOException;
}
