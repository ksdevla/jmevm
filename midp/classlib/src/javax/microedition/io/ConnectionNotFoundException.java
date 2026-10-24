package javax.microedition.io;

import java.io.IOException;

/** The target of a connection cannot be found, or its protocol is not supported. */
public class ConnectionNotFoundException extends IOException {
    public ConnectionNotFoundException() {
    }

    public ConnectionNotFoundException(String s) {
        super(s);
    }
}
