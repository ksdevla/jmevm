package javax.microedition.io;

import java.io.IOException;

/** A server socket. */
public interface ServerSocketConnection extends StreamConnectionNotifier {
    String getLocalAddress() throws IOException;

    int getLocalPort() throws IOException;
}
