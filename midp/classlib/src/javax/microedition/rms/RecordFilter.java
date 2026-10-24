package javax.microedition.rms;

/** Chooses the records an enumeration includes. */
public interface RecordFilter {
    boolean matches(byte[] candidate);
}
