package javax.microedition.media;

/** Something with controls. */
public interface Controllable {
    Control[] getControls();

    Control getControl(String controlType);
}
