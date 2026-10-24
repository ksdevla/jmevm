package com.nokia.mid.ui;

import javax.microedition.lcdui.Graphics;
import javax.microedition.lcdui.Image;

import jmevm.midp.LcduiAccess;

/** Ways into the Nokia UI: a DirectGraphics for a Graphics, and images. */
public class DirectUtils {
    private DirectUtils() {
    }

    public static DirectGraphics getDirectGraphics(Graphics g) {
        if (g == null) {
            throw new NullPointerException();
        }
        return new DirectGraphicsImpl(g);
    }

    /** A mutable image filled with an ARGB colour (it may be transparent). */
    public static Image createImage(int width, int height, int argb) {
        if (width <= 0 || height <= 0) {
            throw new IllegalArgumentException("image size " + width + "x" + height);
        }
        return LcduiAccess.get().createMutable(width, height, argb);
    }

    public static Image createImage(byte[] imageData, int imageOffset, int imageLength) {
        return Image.createImage(imageData, imageOffset, imageLength);
    }
}
