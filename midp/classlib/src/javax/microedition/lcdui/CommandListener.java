package javax.microedition.lcdui;

/** Hears about commands invoked on a displayable. */
public interface CommandListener {
    void commandAction(Command c, Displayable d);
}
