package javax.microedition.io;

import java.io.DataInput;
import java.io.DataOutput;
import java.io.IOException;

/** A datagram: an address and a buffer, read and written as data. */
public interface Datagram extends DataInput, DataOutput {
    String getAddress();

    byte[] getData();

    int getLength();

    int getOffset();

    void setAddress(String addr) throws IOException;

    void setAddress(Datagram reference);

    void setLength(int len);

    void setData(byte[] buffer, int offset, int len);

    void reset();
}
