package javax.microedition.lcdui.game;

import javax.microedition.lcdui.Image;

/** Pixel tests for collisions. */
final class Pixels {
    private static final int[] ONE = new int[1];

    private Pixels() {
    }

    /** Whether pixel (x, y) of the image is not fully transparent. */
    static boolean opaque(Image img, int x, int y) {
        if (x < 0 || y < 0 || x >= img.getWidth() || y >= img.getHeight()) {
            return false;
        }
        synchronized (ONE) {
            img.getRGB(ONE, 0, 1, x, y, 1, 1);
            return (ONE[0] >>> 24) != 0;
        }
    }
}
