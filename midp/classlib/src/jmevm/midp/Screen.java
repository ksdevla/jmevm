package jmevm.midp;

import javax.microedition.lcdui.Image;

/** The display of the platform (natives in src/midp_jme/lcdui.c). */
public final class Screen {
    private Screen() {
    }

    public static native int width();

    public static native int height();

    /** Shows an image of the screen's size as the new frame. */
    public static native void present(Image frame);

    /** The labels of the soft keys, for platforms that show them outside. */
    public static native void softLabels(String left, String right);
}
