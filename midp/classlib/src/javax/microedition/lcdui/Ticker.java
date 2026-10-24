package javax.microedition.lcdui;

/** Text that scrolls across the top of a screen. */
public class Ticker {
    private String text;

    public Ticker(String str) {
        setString(str);
    }

    public void setString(String str) {
        if (str == null) {
            throw new NullPointerException();
        }
        text = str;
        Display.repaintCurrent();
    }

    public String getString() {
        return text;
    }
}
