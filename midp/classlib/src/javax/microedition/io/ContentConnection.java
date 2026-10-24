package javax.microedition.io;

/** A stream connection whose content has a type, an encoding and a length. */
public interface ContentConnection extends StreamConnection {
    String getType();

    String getEncoding();

    long getLength();
}
