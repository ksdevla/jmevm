package javax.microedition.lcdui;

/** The kind of an alert, and its sound. */
public class AlertType {
    public static final AlertType ALARM = new AlertType(81);
    public static final AlertType CONFIRMATION = new AlertType(76);
    public static final AlertType ERROR = new AlertType(57);
    public static final AlertType INFO = new AlertType(72);
    public static final AlertType WARNING = new AlertType(69);

    private final int note;

    protected AlertType() {
        this(72);
    }

    private AlertType(int note) {
        this.note = note;
    }

    /** A short tone; false when there is no sound. */
    public boolean playSound(Display display) {
        if (display == null) {
            throw new NullPointerException();
        }
        if (!jmevm.midp.Audio.available()) {
            return false;
        }
        jmevm.midp.Audio.playTone(note, 150, 80);
        return true;
    }
}
