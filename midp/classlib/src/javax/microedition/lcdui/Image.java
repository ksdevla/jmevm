package javax.microedition.lcdui;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;

import jmevm.midp.Resources;

/**
 * An image: 0xAARRGGBB pixels in a Java array, so they are part of save
 * states. Mutable images (made blank) can be drawn on and are opaque;
 * immutable ones (from PNG data, pixels or other images) may be translucent.
 */
public class Image {
    /* Read and set by the natives in src/midp_jme/lcdui.c. */
    int width;
    int height;
    int[] pixels;
    boolean alpha;

    private boolean mutable;

    private Image() {
    }

    public static Image createImage(int width, int height) {
        if (width <= 0 || height <= 0) {
            throw new IllegalArgumentException("image size " + width + "x" + height);
        }
        Image img = new Image();
        initBlank(img, width, height);
        img.mutable = true;
        return img;
    }

    public static Image createImage(Image source) {
        if (!source.mutable) {
            return source;
        }
        return createImage(source, 0, 0, source.width, source.height, 0);
    }

    public static Image createImage(String name) throws IOException {
        if (name == null) {
            throw new NullPointerException();
        }
        byte[] data = Resources.read(name.startsWith("/") ? name.substring(1) : name);
        if (data == null) {
            throw new IOException("no image " + name);
        }
        Image img = new Image();
        if (!decode(img, data, 0, data.length)) {
            throw new IOException(name + " is not an image");
        }
        return img;
    }

    public static Image createImage(byte[] imageData, int imageOffset, int imageLength) {
        Image img = new Image();
        if (!decode(img, imageData, imageOffset, imageLength)) {
            throw new IllegalArgumentException("not an image");
        }
        return img;
    }

    public static Image createImage(Image image, int x, int y, int width, int height, int transform) {
        if (image == null) {
            throw new NullPointerException();
        }
        if (transform < 0 || transform > 7) {
            throw new IllegalArgumentException("transform " + transform);
        }
        if (width <= 0 || height <= 0) {
            throw new IllegalArgumentException("region size " + width + "x" + height);
        }
        if (!image.mutable && x == 0 && y == 0 && width == image.width && height == image.height && transform == 0) {
            return image;
        }
        Image img = new Image();
        initRegion(img, image, x, y, width, height, transform);
        return img;
    }

    public static Image createImage(InputStream stream) throws IOException {
        if (stream == null) {
            throw new NullPointerException();
        }
        ByteArrayOutputStream bytes = new ByteArrayOutputStream();
        byte[] buf = new byte[4096];
        for (int n; (n = stream.read(buf)) > 0;) {
            bytes.write(buf, 0, n);
        }
        byte[] data = bytes.toByteArray();
        Image img = new Image();
        if (!decode(img, data, 0, data.length)) {
            throw new IOException("not an image");
        }
        return img;
    }

    public static Image createRGBImage(int[] rgb, int width, int height, boolean processAlpha) {
        if (rgb == null) {
            throw new NullPointerException();
        }
        if (width <= 0 || height <= 0) {
            throw new IllegalArgumentException("image size " + width + "x" + height);
        }
        Image img = new Image();
        initRGB(img, rgb, width, height, processAlpha);
        return img;
    }

    public Graphics getGraphics() {
        if (!mutable) {
            throw new IllegalStateException("the image is immutable");
        }
        return new Graphics(this, false);
    }

    public int getWidth() {
        return width;
    }

    public int getHeight() {
        return height;
    }

    public boolean isMutable() {
        return mutable;
    }

    public void getRGB(int[] rgbData, int offset, int scanlength, int x, int y, int width, int height) {
        if (rgbData == null) {
            throw new NullPointerException();
        }
        if (x < 0 || y < 0 || width < 0 || height < 0 || x + width > this.width || y + height > this.height) {
            throw new IllegalArgumentException("region outside the image");
        }
        if (Math.abs(scanlength) < width) {
            throw new IllegalArgumentException("scanlength " + scanlength + " for width " + width);
        }
        if (width == 0 || height == 0) {
            return;
        }
        long first = offset + (scanlength < 0 ? (long) (height - 1) * scanlength : 0);
        long last = offset + (scanlength < 0 ? 0 : (long) (height - 1) * scanlength) + width - 1;
        if (first < 0 || last >= rgbData.length) {
            throw new ArrayIndexOutOfBoundsException();
        }
        for (int row = 0; row < height; row++) {
            System.arraycopy(pixels, (y + row) * this.width + x, rgbData, offset + row * scanlength, width);
        }
    }

    private static native boolean decode(Image img, byte[] data, int off, int len);

    private static native void initRGB(Image img, int[] rgb, int w, int h, boolean alpha);

    private static native void initBlank(Image img, int w, int h);

    private static native void initRegion(Image dst, Image src, int x, int y, int w, int h, int transform);
}
