package jmevm.test.draw;

import java.io.IOException;
import java.io.InputStream;

import javax.microedition.lcdui.Display;
import javax.microedition.lcdui.Font;
import javax.microedition.lcdui.Graphics;
import javax.microedition.lcdui.Image;
import javax.microedition.lcdui.game.GameCanvas;
import javax.microedition.lcdui.game.Sprite;
import javax.microedition.midlet.MIDlet;

/**
 * Drawing: Graphics primitives, clipping and translation, colours, images
 * (mutable, RGB, PNG from the jar, regions and transforms), fonts and text.
 * Each check draws into an offscreen image and compares pixels with what
 * the MIDP 2.0 specification requires. Then a GameCanvas animates a scene
 * and moves a box with the keys, printing each fire press.
 */
public class DrawMIDlet extends MIDlet {
    static final int WHITE = 0xFFFFFFFF;

    protected void startApp() {
        try {
            primitives();
            clipAndTranslate();
            colours();
            regions();
            rgbImages();
            pngImages();
            fonts();
            arcsAndTriangles();
        } catch (Throwable t) {
            Check.yes("no exception: " + t, false);
        }
        Check.summary();
        Scene scene = new Scene();
        Display.getDisplay(this).setCurrent(scene);
        new Thread(scene).start();
    }

    protected void pauseApp() {
    }

    protected void destroyApp(boolean unconditional) {
    }

    // ---- helpers

    static int pixel(Image im, int x, int y) {
        int[] p = new int[1];
        im.getRGB(p, 0, 1, x, y, 1, 1);
        return p[0];
    }

    /** Number of pixels of the image that are not white. */
    static int marked(Image im) {
        int w = im.getWidth(), h = im.getHeight();
        int[] p = new int[w * h];
        im.getRGB(p, 0, w, 0, 0, w, h);
        int n = 0;
        for (int i = 0; i < p.length; i++) {
            if (p[i] != WHITE) {
                n++;
            }
        }
        return n;
    }

    /** Whether every non-white pixel lies inside the rectangle. */
    static boolean markedWithin(Image im, int x, int y, int w, int h) {
        int iw = im.getWidth(), ih = im.getHeight();
        int[] p = new int[iw * ih];
        im.getRGB(p, 0, iw, 0, 0, iw, ih);
        for (int i = 0; i < p.length; i++) {
            int px = i % iw, py = i / iw;
            if (p[i] != WHITE && (px < x || px >= x + w || py < y || py >= y + h)) {
                return false;
            }
        }
        return true;
    }

    /** The colour of source pixel (x, y) in the 3x2 test image. */
    static int source(int x, int y) {
        return 0xFF000000 | ((x * 40 + 10) << 16) | ((y * 80 + 20) << 8) | 5;
    }

    static Image sourceImage() {
        int[] argb = new int[6];
        for (int y = 0; y < 2; y++) {
            for (int x = 0; x < 3; x++) {
                argb[y * 3 + x] = source(x, y);
            }
        }
        return Image.createRGBImage(argb, 3, 2, false);
    }

    // ---- checks

    static void primitives() {
        Image im = Image.createImage(20, 10);
        Check.yes("mutable image", im.isMutable());
        Check.eq("mutable image size", im.getWidth() * 100 + im.getHeight(), 2010);
        Check.eq("mutable image starts white", marked(im), 0);

        Graphics g = im.getGraphics();
        g.setColor(0xFF0000);
        g.fillRect(2, 3, 4, 5);
        Check.eq("fillRect area", marked(im), 20);
        Check.eq("fillRect first pixel", pixel(im, 2, 3), 0xFFFF0000);
        Check.eq("fillRect last pixel", pixel(im, 5, 7), 0xFFFF0000);
        Check.eq("fillRect excludes right edge", pixel(im, 6, 7), WHITE);
        Check.eq("fillRect excludes bottom edge", pixel(im, 5, 8), WHITE);

        im = Image.createImage(10, 10);
        g = im.getGraphics();
        g.setColor(0);
        g.drawRect(1, 1, 3, 2);   // covers x 1..4, y 1..3
        Check.eq("drawRect outline", marked(im), 10);
        Check.yes("drawRect corners", pixel(im, 1, 1) != WHITE && pixel(im, 4, 1) != WHITE
                && pixel(im, 1, 3) != WHITE && pixel(im, 4, 3) != WHITE);
        Check.yes("drawRect inside untouched", pixel(im, 2, 2) == WHITE && pixel(im, 3, 2) == WHITE);

        im = Image.createImage(10, 10);
        g = im.getGraphics();
        g.setColor(0);
        g.drawLine(0, 0, 5, 0);
        Check.eq("horizontal line includes both ends", marked(im), 6);
        im = Image.createImage(10, 10);
        g = im.getGraphics();
        g.setColor(0);
        g.drawLine(0, 0, 3, 3);
        boolean diagonal = marked(im) == 4;
        for (int i = 0; i < 4; i++) {
            diagonal &= pixel(im, i, i) != WHITE;
        }
        Check.yes("diagonal line", diagonal);

        im = Image.createImage(8, 8);
        g = im.getGraphics();
        g.setColor(0);
        g.drawLine(2, 2, 2, 2);
        Check.eq("one-pixel line", marked(im), 1);

        Check.fails("graphics of an immutable image", IllegalStateException.class, new Check.Action() {
            public void run() {
                sourceImage().getGraphics();
            }
        });
        Check.fails("zero-width image", IllegalArgumentException.class, new Check.Action() {
            public void run() {
                Image.createImage(0, 10);
            }
        });

        // copyArea
        im = Image.createImage(10, 10);
        g = im.getGraphics();
        g.setColor(0x00FF00);
        g.fillRect(0, 0, 2, 1);
        g.copyArea(0, 0, 2, 1, 5, 6, Graphics.TOP | Graphics.LEFT);
        Check.yes("copyArea", pixel(im, 5, 6) == 0xFF00FF00 && pixel(im, 6, 6) == 0xFF00FF00
                && marked(im) == 4);
    }

    static void clipAndTranslate() {
        Image im = Image.createImage(20, 20);
        Graphics g = im.getGraphics();
        Check.eq("initial clip is the image", g.getClipWidth() * 100 + g.getClipHeight(), 2020);
        g.setColor(0);
        g.setClip(5, 5, 3, 3);
        g.fillRect(0, 0, 20, 20);
        Check.eq("fill inside a clip", marked(im), 9);
        Check.eq("clip", g.getClipX() * 1000 + g.getClipY() * 100 + g.getClipWidth() * 10 + g.getClipHeight(),
                5533);
        g.clipRect(6, 6, 10, 10);
        Check.eq("clipRect intersects", g.getClipX() * 1000 + g.getClipY() * 100 + g.getClipWidth() * 10
                + g.getClipHeight(), 6622);

        im = Image.createImage(20, 20);
        g = im.getGraphics();
        g.setColor(0);
        g.translate(2, 1);
        g.fillRect(0, 0, 1, 1);
        Check.yes("translate moves drawing", pixel(im, 2, 1) != WHITE && marked(im) == 1);
        Check.eq("translation", g.getTranslateX() * 10 + g.getTranslateY(), 21);
        g.translate(1, 2);
        Check.eq("translations add up", g.getTranslateX() * 10 + g.getTranslateY(), 33);
        g.setClip(0, 0, 2, 2);
        Check.eq("clip is in translated coordinates", g.getClipX(), 0);
        g.fillRect(-5, -5, 20, 20);
        Check.yes("translated clip", pixel(im, 3, 3) != WHITE && pixel(im, 4, 4) != WHITE
                && pixel(im, 5, 5) == WHITE && marked(im) == 5);
    }

    static void colours() {
        Image im = Image.createImage(4, 4);
        Graphics g = im.getGraphics();
        g.setColor(0x123456);
        Check.eq("getColor", g.getColor(), 0x123456);
        Check.eq("components", g.getRedComponent() * 65536 + g.getGreenComponent() * 256
                + g.getBlueComponent(), 0x123456);
        g.setColor(1, 2, 3);
        Check.eq("setColor(r, g, b)", g.getColor(), 0x010203);
        g.setColor(0xAB123456);
        Check.eq("setColor ignores the top byte", g.getColor(), 0x123456);
        g.setGrayScale(100);
        Check.eq("setGrayScale", g.getColor(), 0x646464);
        Check.fails("colour component above 255", IllegalArgumentException.class, new Check.Action() {
            public void run() {
                Image.createImage(1, 1).getGraphics().setColor(256, 0, 0);
            }
        });
        g.setStrokeStyle(Graphics.DOTTED);
        Check.eq("stroke style", g.getStrokeStyle(), Graphics.DOTTED);
        g.setColor(0x00FF00);
        g.fillRect(0, 0, 1, 1);
        Check.eq("pixels are opaque", pixel(im, 0, 0), 0xFF00FF00);
    }

    /** The source pixel that destination pixel (x, y) shows for a transform of a w x h region. */
    static int transformed(int transform, int x, int y, int w, int h) {
        switch (transform) {
        case Sprite.TRANS_NONE: return source(x, y);
        case Sprite.TRANS_MIRROR: return source(w - 1 - x, y);
        case Sprite.TRANS_MIRROR_ROT180: return source(x, h - 1 - y);
        case Sprite.TRANS_ROT180: return source(w - 1 - x, h - 1 - y);
        case Sprite.TRANS_ROT90: return source(y, h - 1 - x);
        case Sprite.TRANS_ROT270: return source(w - 1 - y, x);
        case Sprite.TRANS_MIRROR_ROT90: return source(w - 1 - y, h - 1 - x);
        default: return source(y, x);   // TRANS_MIRROR_ROT270
        }
    }

    static void regions() {
        Image src = sourceImage();
        String[] names = {"none", "mirror rot180", "mirror", "rot180", "mirror rot270", "rot90", "rot270",
                "mirror rot90"};
        for (int t = 0; t < 8; t++) {
            boolean turned = t >= 4;   // the transforms that swap width and height
            int w = turned ? 2 : 3, h = turned ? 3 : 2;
            Image im = Image.createImage(4, 4);
            Graphics g = im.getGraphics();
            g.drawRegion(src, 0, 0, 3, 2, t, 0, 0, Graphics.TOP | Graphics.LEFT);
            boolean same = marked(im) == w * h;
            for (int y = 0; y < h; y++) {
                for (int x = 0; x < w; x++) {
                    same &= pixel(im, x, y) == transformed(t, x, y, 3, 2);
                }
            }
            Check.yes("drawRegion " + names[t], same);
        }

        Image im = Image.createImage(10, 10);
        im.getGraphics().drawRegion(src, 0, 0, 3, 2, Sprite.TRANS_NONE, 5, 5,
                Graphics.BOTTOM | Graphics.RIGHT);
        Check.yes("BOTTOM|RIGHT anchor", pixel(im, 2, 3) == source(0, 0) && pixel(im, 4, 4) == source(2, 1)
                && marked(im) == 6);
        im = Image.createImage(10, 10);
        im.getGraphics().drawRegion(src, 0, 0, 3, 2, Sprite.TRANS_NONE, 5, 5,
                Graphics.HCENTER | Graphics.VCENTER);
        Check.yes("HCENTER|VCENTER anchor", pixel(im, 4, 4) == source(0, 0) && pixel(im, 6, 5) == source(2, 1)
                && marked(im) == 6);
        im = Image.createImage(10, 10);
        im.getGraphics().drawImage(src, 1, 2, 0);
        Check.yes("anchor 0 is TOP|LEFT", pixel(im, 1, 2) == source(0, 0) && marked(im) == 6);
        Check.fails("drawImage with BASELINE", IllegalArgumentException.class, new Check.Action() {
            public void run() {
                Image.createImage(4, 4).getGraphics().drawImage(sourceImage(), 0, 0, Graphics.BASELINE);
            }
        });
        Check.fails("region outside the image", IllegalArgumentException.class, new Check.Action() {
            public void run() {
                Image.createImage(4, 4).getGraphics().drawRegion(sourceImage(), 2, 0, 3, 2, 0, 0, 0, 0);
            }
        });

        // A transformed copy of a region: (1, 0, 2, 2) rotated 90 degrees.
        Image part = Image.createImage(src, 1, 0, 2, 2, Sprite.TRANS_ROT90);
        Check.yes("createImage(region, rot90)", !part.isMutable() && part.getWidth() == 2
                && pixel(part, 0, 0) == source(1, 1) && pixel(part, 1, 0) == source(1, 0)
                && pixel(part, 0, 1) == source(2, 1) && pixel(part, 1, 1) == source(2, 0));
    }

    static void rgbImages() {
        int[] argb = {0x00FF0000, 0xFF0000FF};
        Image alpha = Image.createRGBImage(argb, 2, 1, true);
        Image opaque = Image.createRGBImage(argb, 2, 1, false);
        Image im = Image.createImage(4, 2);
        Graphics g = im.getGraphics();
        g.drawImage(alpha, 0, 0, 0);
        g.drawImage(opaque, 0, 1, 0);
        Check.eq("transparent pixel leaves the background", pixel(im, 0, 0), WHITE);
        Check.eq("opaque pixel", pixel(im, 1, 0), 0xFF0000FF);
        Check.eq("without alpha processing the pixel is opaque", pixel(im, 0, 1), 0xFFFF0000);
        Check.eq("getRGB keeps transparency", pixel(alpha, 0, 0) >>> 24, 0);
        Check.eq("getRGB of an opaque RGB image", pixel(opaque, 0, 0), 0xFFFF0000);

        im = Image.createImage(4, 4);
        im.getGraphics().drawRGB(new int[] {0, 0xFF00FF00, 0xFF00FF00, 0}, 0, 2, 1, 1, 2, 2, true);
        Check.yes("drawRGB", pixel(im, 1, 1) == WHITE && pixel(im, 2, 1) == 0xFF00FF00
                && pixel(im, 1, 2) == 0xFF00FF00 && marked(im) == 2);

        int[] buf = new int[13];
        sourceImage().getRGB(buf, 5, 4, 0, 0, 2, 2);
        Check.yes("getRGB offset and scanlength", buf[4] == 0 && buf[5] == source(0, 0) && buf[6] == source(1, 0)
                && buf[7] == 0 && buf[9] == source(0, 1) && buf[10] == source(1, 1));
        Check.fails("getRGB scanlength too small", IllegalArgumentException.class, new Check.Action() {
            public void run() {
                sourceImage().getRGB(new int[6], 0, 1, 0, 0, 3, 2);
            }
        });
    }

    static void pngImages() throws IOException {
        int r = 0xFFFF0000, g = 0xFF00FF00, b = 0xFF0000FF;
        int[] want = {r, g, b, 0, g, b, 0, r, b, 0, r, g};   // 0: transparent
        Image png = Image.createImage("/pattern.png");
        Check.yes("PNG from the jar", !png.isMutable() && png.getWidth() == 4 && png.getHeight() == 3);
        int[] got = new int[12];
        png.getRGB(got, 0, 4, 0, 0, 4, 3);
        boolean same = true;
        for (int i = 0; i < 12; i++) {
            same &= want[i] == 0 ? got[i] >>> 24 == 0 : got[i] == want[i];
        }
        Check.yes("PNG pixels, palette and tRNS", same);

        InputStream in = DrawMIDlet.class.getResourceAsStream("/pattern.png");
        byte[] data = new byte[4096];
        int n = 0;
        for (int k; (k = in.read(data, n, data.length - n)) > 0;) {
            n += k;
        }
        in.close();
        byte[] padded = new byte[n + 3];
        System.arraycopy(data, 0, padded, 3, n);
        Image fromBytes = Image.createImage(padded, 3, n);
        fromBytes.getRGB(got, 0, 4, 0, 0, 4, 3);
        Check.yes("PNG from bytes at an offset", got[0] == r && got[11] == g);
        Check.fails("missing image", IOException.class, new Check.Action() {
            public void run() throws IOException {
                Image.createImage("/missing.png");
            }
        });
        Check.fails("bytes that are no image", IllegalArgumentException.class, new Check.Action() {
            public void run() {
                Image.createImage(new byte[] {1, 2, 3, 4, 5, 6, 7, 8}, 0, 8);
            }
        });
    }

    static void fonts() {
        Font f = Font.getFont(Font.FACE_SYSTEM, Font.STYLE_BOLD, Font.SIZE_LARGE);
        Check.yes("font attributes", f.getFace() == Font.FACE_SYSTEM && f.getStyle() == Font.STYLE_BOLD
                && f.getSize() == Font.SIZE_LARGE && f.isBold() && !f.isItalic() && !f.isPlain());
        Check.yes("default font is plain", Font.getDefaultFont().isPlain());
        Check.yes("static text font", Font.getFont(Font.FONT_STATIC_TEXT) != null);
        Check.eq("empty string width", f.stringWidth(""), 0);
        Check.eq("substringWidth", f.substringWidth("xabcx", 1, 3), f.stringWidth("abc"));
        Check.eq("charsWidth", f.charsWidth(new char[] {'-', 'a', 'b', 'c'}, 1, 3), f.stringWidth("abc"));
        Check.yes("height and baseline", f.getHeight() > 0 && f.getBaselinePosition() > 0
                && f.getBaselinePosition() <= f.getHeight());

        Font d = Font.getDefaultFont();
        Image im = Image.createImage(60, 40);
        Graphics g = im.getGraphics();
        g.setFont(d);
        g.setColor(0);
        g.drawString("Hi", 5, 5, Graphics.TOP | Graphics.LEFT);
        Check.yes("text drawn", marked(im) > 0);
        Check.yes("text inside its box", markedWithin(im, 5, 5, d.stringWidth("Hi"), d.getHeight()));
        int topLeft = marked(im);

        im = Image.createImage(60, 40);
        g = im.getGraphics();
        g.setColor(0);
        g.drawString("Hi", 5, 30, Graphics.BASELINE | Graphics.LEFT);
        int top = 30 - d.getBaselinePosition();
        Check.yes("BASELINE anchor", marked(im) == topLeft
                && markedWithin(im, 5, top, d.stringWidth("Hi"), d.getHeight()));
        im = Image.createImage(60, 40);
        g = im.getGraphics();
        g.setColor(0);
        g.drawString("Hi", 40, 5, Graphics.TOP | Graphics.RIGHT);
        Check.yes("RIGHT anchor", marked(im) == topLeft
                && markedWithin(im, 40 - d.stringWidth("Hi"), 5, d.stringWidth("Hi"), d.getHeight()));
        Check.fails("drawString with VCENTER", IllegalArgumentException.class, new Check.Action() {
            public void run() {
                Image.createImage(4, 4).getGraphics().drawString("x", 0, 0, Graphics.VCENTER);
            }
        });
        Check.fails("drawString(null)", NullPointerException.class, new Check.Action() {
            public void run() {
                Image.createImage(4, 4).getGraphics().drawString(null, 0, 0, 0);
            }
        });
    }

    static void arcsAndTriangles() {
        Image im = Image.createImage(8, 8);
        Graphics g = im.getGraphics();
        g.setColor(0);
        g.fillTriangle(0, 0, 6, 0, 0, 6);
        Check.yes("fillTriangle", pixel(im, 0, 0) != WHITE && pixel(im, 1, 1) != WHITE
                && pixel(im, 5, 5) == WHITE && pixel(im, 7, 7) == WHITE);

        im = Image.createImage(9, 9);
        g = im.getGraphics();
        g.setColor(0);
        g.fillArc(0, 0, 9, 9, 0, 360);
        Check.yes("fillArc circle", pixel(im, 4, 4) != WHITE && pixel(im, 0, 0) == WHITE
                && pixel(im, 8, 8) == WHITE && pixel(im, 4, 0) != WHITE);
        im = Image.createImage(9, 9);
        g = im.getGraphics();
        g.setColor(0);
        g.fillArc(0, 0, 9, 9, 0, 90);   // the quarter up and to the right of the centre
        Check.yes("fillArc quarter", pixel(im, 6, 2) != WHITE && pixel(im, 2, 6) == WHITE
                && pixel(im, 2, 2) == WHITE);
        im = Image.createImage(9, 9);
        g = im.getGraphics();
        g.setColor(0);
        g.drawArc(0, 0, 8, 8, 0, 360);
        Check.yes("drawArc outline", pixel(im, 4, 4) == WHITE && marked(im) > 8);
        im = Image.createImage(20, 20);
        g = im.getGraphics();
        g.setColor(0);
        g.fillRoundRect(2, 2, 10, 10, 6, 6);
        Check.yes("fillRoundRect", pixel(im, 2, 2) == WHITE && pixel(im, 7, 7) != WHITE
                && pixel(im, 2, 7) != WHITE && markedWithin(im, 2, 2, 10, 10));
    }

    /** The animated part: a scene with a box that the keys move. */
    static class Scene extends GameCanvas implements Runnable {
        private Image pattern;
        private int x = 100, y = 120, frame;

        Scene() {
            super(false);
            try {
                pattern = Image.createImage("/pattern.png");
            } catch (IOException e) {
                pattern = Image.createImage(4, 3);
            }
        }

        protected void keyPressed(int keyCode) {
            System.out.println("key " + keyCode + " pressed, game action " + getGameAction(keyCode));
        }

        public void run() {
            Graphics g = getGraphics();
            boolean fireWasDown = false;
            for (;;) {
                int keys = getKeyStates();
                if ((keys & UP_PRESSED) != 0) y -= 4;
                if ((keys & DOWN_PRESSED) != 0) y += 4;
                if ((keys & LEFT_PRESSED) != 0) x -= 4;
                if ((keys & RIGHT_PRESSED) != 0) x += 4;
                boolean fire = (keys & FIRE_PRESSED) != 0;
                if (fire && !fireWasDown) {
                    System.out.println("fire at " + x + "," + y + " in frame " + frame);
                }
                fireWasDown = fire;
                paintScene(g);
                flushGraphics();
                frame++;
                try {
                    Thread.sleep(40);
                } catch (InterruptedException e) {
                    return;
                }
            }
        }

        private void paintScene(Graphics g) {
            int w = getWidth(), h = getHeight();
            g.setColor(0x102030);
            g.fillRect(0, 0, w, h);
            g.setColor(0x3060C0);
            for (int i = 0; i < 8; i++) {
                int a = (frame * 6 + i * 45) % 360;
                g.fillArc(20 + i * 24, 20, 20, 20, a, 120);
            }
            g.setColor(0xE0E0E0);
            g.drawRoundRect(10, 50, w - 20, 40, 12, 12);
            g.setFont(Font.getFont(Font.FACE_SYSTEM, Font.STYLE_BOLD, Font.SIZE_MEDIUM));
            g.drawString("jmevm draw", w / 2, 60, Graphics.TOP | Graphics.HCENTER);
            for (int t = 0; t < 8; t++) {
                g.drawRegion(pattern, 0, 0, 4, 3, t, 20 + t * 26, 100, Graphics.TOP | Graphics.LEFT);
            }
            g.setColor(0xFF8000);
            g.fillTriangle(x, y, x + 16, y, x + 8, y - 14);
            g.fillRect(x, y, 16, 16);
            g.setColor(0xFFFFFF);
            g.drawString("frame " + frame, 4, h - 4, Graphics.BOTTOM | Graphics.LEFT);
        }
    }
}
