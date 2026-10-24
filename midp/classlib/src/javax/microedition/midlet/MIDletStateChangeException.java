package javax.microedition.midlet;

/** A MIDlet cannot change its state now (from startApp or destroyApp). */
public class MIDletStateChangeException extends Exception {
    public MIDletStateChangeException() {
    }

    public MIDletStateChangeException(String s) {
        super(s);
    }
}
