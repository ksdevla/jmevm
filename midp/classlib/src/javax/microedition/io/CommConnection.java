package javax.microedition.io;

/** A serial port. */
public interface CommConnection extends StreamConnection {
    int getBaudRate();

    int setBaudRate(int baudrate);
}
