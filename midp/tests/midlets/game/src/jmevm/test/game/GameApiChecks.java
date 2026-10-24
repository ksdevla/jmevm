package jmevm.test.game;

import javax.microedition.lcdui.Graphics;
import javax.microedition.lcdui.Image;
import javax.microedition.lcdui.game.LayerManager;
import javax.microedition.lcdui.game.Sprite;
import javax.microedition.lcdui.game.TiledLayer;

/** The game API (javax.microedition.lcdui.game), against the MIDP 2.0 specification. */
final class GameApiChecks {
    private GameApiChecks() {
    }

    static Image solid(int w, int h, int argb) {
        int[] p = new int[w * h];
        for (int i = 0; i < p.length; i++) {
            p[i] = argb;
        }
        return Image.createRGBImage(p, w, h, true);
    }

    /** 4x4, opaque only in the 2x2 square at its top left. */
    static Image corner() {
        int[] p = new int[16];
        p[0] = p[1] = p[4] = p[5] = 0xFF00FF00;
        return Image.createRGBImage(p, 4, 4, true);
    }

    static int pixel(Image im, int x, int y) {
        int[] p = new int[1];
        im.getRGB(p, 0, 1, x, y, 1, 1);
        return p[0];
    }

    static void run() {
        sprites();
        collisions();
        tiledLayers();
        layerManager();
    }

    static void sprites() {
        final Image frames = solid(36, 16, 0xFF808080);   // 3 x 2 frames of 12 x 8
        final Sprite s = new Sprite(frames, 12, 8);
        Check.eq("sprite raw frames", s.getRawFrameCount(), 6);
        Check.eq("sprite size", s.getWidth() * 100 + s.getHeight(), 1208);
        Check.eq("sprite default sequence", s.getFrameSequenceLength(), 6);
        s.nextFrame();
        Check.eq("sprite nextFrame", s.getFrame(), 1);
        s.setFrame(0);
        s.prevFrame();
        Check.eq("sprite prevFrame wraps", s.getFrame(), 5);
        s.setFrameSequence(new int[] {5, 1, 1, 3});
        Check.eq("sprite sequence resets the frame", s.getFrame(), 0);
        s.nextFrame();
        s.nextFrame();
        s.nextFrame();
        Check.eq("sprite sequence index", s.getFrame(), 3);
        s.nextFrame();
        Check.eq("sprite sequence wraps", s.getFrame(), 0);
        Check.eq("sprite sequence length", s.getFrameSequenceLength(), 4);
        Check.fails("sprite frame outside the sequence", IndexOutOfBoundsException.class, new Check.Action() {
            public void run() {
                s.setFrame(4);
            }
        });
        Check.fails("sprite sequence with a missing frame", ArrayIndexOutOfBoundsException.class,
                new Check.Action() {
                    public void run() {
                        s.setFrameSequence(new int[] {0, 6});
                    }
                });
        s.setFrameSequence(null);
        Check.eq("sprite default sequence again", s.getFrameSequenceLength(), 6);
        Check.fails("sprite frame size not dividing the image", IllegalArgumentException.class,
                new Check.Action() {
                    public void run() {
                        new Sprite(frames, 10, 8);
                    }
                });

        // The reference pixel stays where it is through every transform; the
        // position moves. Frame 12 x 8, reference pixel (3, 2) at (100, 50).
        s.defineReferencePixel(3, 2);
        s.setRefPixelPosition(100, 50);
        Check.eq("sprite position from the reference pixel", s.getX() * 1000 + s.getY(), 97048);
        int[] transforms = {Sprite.TRANS_NONE, Sprite.TRANS_MIRROR, Sprite.TRANS_ROT180, Sprite.TRANS_MIRROR_ROT180,
                Sprite.TRANS_ROT90, Sprite.TRANS_ROT270, Sprite.TRANS_MIRROR_ROT90, Sprite.TRANS_MIRROR_ROT270};
        String[] names = {"none", "mirror", "rot180", "mirror rot180", "rot90", "rot270", "mirror rot90",
                "mirror rot270"};
        int[] positions = {97048, 92048, 92045, 97045, 95047, 98042, 95042, 98047};
        for (int i = 0; i < 8; i++) {
            s.setTransform(transforms[i]);
            Check.eq("sprite " + names[i] + " position", s.getX() * 1000 + s.getY(), positions[i]);
            Check.eq("sprite " + names[i] + " reference pixel", s.getRefPixelX() * 1000 + s.getRefPixelY(),
                    100050);
        }
        s.setTransform(Sprite.TRANS_ROT90);
        Check.eq("sprite rotated size", s.getWidth() * 100 + s.getHeight(), 812);
        s.setTransform(Sprite.TRANS_NONE);
        s.setImage(solid(24, 24, 0xFF0000FF), 12, 12);
        Check.eq("sprite setImage frames", s.getRawFrameCount(), 4);
        Check.eq("sprite setImage size", s.getWidth() * 100 + s.getHeight(), 1212);

        s.setPosition(7, 9);
        s.setTransform(Sprite.TRANS_MIRROR);
        s.setVisible(false);
        Sprite copy = new Sprite(s);
        Check.yes("sprite copy", copy.getX() == s.getX() && copy.getY() == s.getY() && !copy.isVisible()
                && copy.getRawFrameCount() == 4 && copy.getWidth() == 12);
        s.move(3, -4);
        Check.eq("sprite move", s.getX() * 1000 + s.getY(), (copy.getX() + 3) * 1000 + copy.getY() - 4);
    }

    static void collisions() {
        Sprite a = new Sprite(corner());   // opaque at (0..1, 0..1)
        Sprite probe = new Sprite(solid(1, 1, 0xFFFF0000));
        Image dot = solid(1, 1, 0xFFFF0000);

        probe.setPosition(1, 1);
        Check.yes("collide pixel on opaque", probe.collidesWith(a, true));
        probe.setPosition(2, 2);
        Check.yes("collide pixel on transparent", !probe.collidesWith(a, true));
        Check.yes("collide rectangles", probe.collidesWith(a, false));
        probe.setPosition(4, 0);
        Check.yes("collide just outside", !probe.collidesWith(a, false));

        a.setTransform(Sprite.TRANS_MIRROR);
        a.setPosition(0, 0);   // opaque now at (2..3, 0..1)
        probe.setPosition(2, 0);
        boolean mirrorOpaque = probe.collidesWith(a, true);
        probe.setPosition(0, 0);
        Check.yes("collide mirrored", mirrorOpaque && !probe.collidesWith(a, true));
        a.setTransform(Sprite.TRANS_ROT270);
        a.setPosition(0, 0);   // opaque now at (0..1, 2..3)
        probe.setPosition(1, 3);
        boolean rotatedOpaque = probe.collidesWith(a, true);
        probe.setPosition(3, 1);
        Check.yes("collide rotated", rotatedOpaque && !probe.collidesWith(a, true));

        a.setTransform(Sprite.TRANS_NONE);
        a.setPosition(0, 0);
        Check.yes("collide with an image", a.collidesWith(dot, 1, 1, true) && !a.collidesWith(dot, 3, 3, true)
                && a.collidesWith(dot, 3, 3, false));
        a.defineCollisionRectangle(0, 0, 2, 2);
        probe.setPosition(3, 3);
        boolean outside = probe.collidesWith(a, false);
        probe.setPosition(1, 1);
        Check.yes("collision rectangle", !outside && probe.collidesWith(a, false));
        a.setVisible(false);
        Check.yes("invisible sprites do not collide", !probe.collidesWith(a, false));

        // Tiles 4x4: tile 1 opaque, tile 2 opaque only at its top-left pixel.
        int[] p = new int[32];
        for (int y = 0; y < 4; y++) {
            for (int x = 0; x < 4; x++) {
                p[y * 8 + x] = 0xFF0000FF;
            }
        }
        p[4] = 0xFFFFFF00;
        TiledLayer t = new TiledLayer(3, 3, Image.createRGBImage(p, 8, 4, true), 4, 4);
        t.setCell(1, 1, 1);   // x 4..7, y 4..7
        t.setCell(2, 2, 2);   // x 8..11, y 8..11
        probe.setPosition(0, 0);
        Check.yes("collide empty cell", !probe.collidesWith(t, false));
        probe.setPosition(5, 5);
        Check.yes("collide tile", probe.collidesWith(t, false) && probe.collidesWith(t, true));
        probe.setPosition(9, 9);
        boolean rectOnly = probe.collidesWith(t, false) && !probe.collidesWith(t, true);
        probe.setPosition(8, 8);
        Check.yes("collide tile pixels", rectOnly && probe.collidesWith(t, true));
        int animated = t.createAnimatedTile(1);
        t.setCell(0, 2, animated);   // x 0..3, y 8..11
        probe.setPosition(1, 9);
        boolean before = probe.collidesWith(t, true);
        t.setAnimatedTile(animated, 2);
        Check.yes("collide animated tile", before && !probe.collidesWith(t, true));
    }

    static void tiledLayers() {
        final TiledLayer t = new TiledLayer(10, 6, solid(32, 16, 0xFF00FF00), 16, 16);
        Check.eq("tiled size", t.getWidth() * 1000 + t.getHeight(), 160096);
        Check.eq("tiled grid", t.getColumns() * 100 + t.getRows() * 10, 1060);
        Check.eq("tiled cell size", t.getCellWidth() * 100 + t.getCellHeight(), 1616);
        Check.eq("tiled cells start empty", t.getCell(3, 3), 0);
        final int anim = t.createAnimatedTile(2);
        Check.eq("tiled first animated tile", anim, -1);
        Check.eq("tiled second animated tile", t.createAnimatedTile(1), -2);
        Check.eq("tiled animated tile", t.getAnimatedTile(anim), 2);
        t.setAnimatedTile(anim, 1);
        Check.eq("tiled animated tile set", t.getAnimatedTile(anim), 1);
        t.setCell(4, 2, anim);
        Check.eq("tiled cell", t.getCell(4, 2), -1);
        t.fillCells(0, 0, 3, 2, 2);
        Check.eq("tiled fillCells", t.getCell(2, 1) * 10 + t.getCell(3, 1), 20);
        Check.fails("tiled cell outside", IndexOutOfBoundsException.class, new Check.Action() {
            public void run() {
                t.setCell(10, 0, 1);
            }
        });
        Check.fails("tiled tile that does not exist", IndexOutOfBoundsException.class, new Check.Action() {
            public void run() {
                t.setCell(0, 0, 3);
            }
        });
        Check.fails("tiled animated tile that does not exist", IndexOutOfBoundsException.class,
                new Check.Action() {
                    public void run() {
                        t.setCell(0, 0, -3);
                    }
                });
        Check.fails("tiled animated tile of a missing tile", IndexOutOfBoundsException.class,
                new Check.Action() {
                    public void run() {
                        t.createAnimatedTile(3);
                    }
                });
        t.setStaticTileSet(solid(48, 16, 0xFFFF0000), 16, 16);   // more tiles: cells stay
        Check.eq("tiled more tiles keep the cells", t.getCell(2, 1), 2);
        t.setStaticTileSet(solid(16, 16, 0xFFFF0000), 16, 16);   // fewer: everything resets
        Check.yes("tiled fewer tiles reset the cells", t.getCell(2, 1) == 0 && t.getCell(4, 2) == 0);
        Check.fails("tiled animated tiles are gone", IndexOutOfBoundsException.class, new Check.Action() {
            public void run() {
                t.getAnimatedTile(anim);
            }
        });
    }

    static void layerManager() {
        Sprite a = new Sprite(solid(2, 2, 0xFF000000));
        Sprite b = new Sprite(solid(2, 2, 0xFF000000));
        Sprite c = new Sprite(solid(2, 2, 0xFF000000));
        final LayerManager lm = new LayerManager();
        lm.append(a);
        lm.append(b);
        Check.yes("lm append", lm.getSize() == 2 && lm.getLayerAt(0) == a && lm.getLayerAt(1) == b);
        lm.insert(c, 0);
        Check.yes("lm insert", lm.getSize() == 3 && lm.getLayerAt(0) == c && lm.getLayerAt(1) == a);
        lm.append(a);
        Check.yes("lm append again moves", lm.getSize() == 3 && lm.getLayerAt(2) == a && lm.getLayerAt(1) == b);
        lm.remove(b);
        Check.yes("lm remove", lm.getSize() == 2 && lm.getLayerAt(1) == a);
        final Sprite d = new Sprite(solid(1, 1, 0xFF000000));
        Check.fails("lm insert past the end", IndexOutOfBoundsException.class, new Check.Action() {
            public void run() {
                lm.insert(d, 5);
            }
        });

        // Drawing: index 0 on top; the view window maps to (3, 3) on screen.
        LayerManager scene = new LayerManager();
        Sprite top = new Sprite(solid(1, 1, 0xFFFF0000));
        top.setPosition(2, 2);
        Sprite bottom = new Sprite(solid(4, 4, 0xFF0000FF));
        bottom.setPosition(1, 1);
        Sprite far = new Sprite(solid(1, 1, 0xFF00FF00));
        far.setPosition(8, 8);
        scene.append(top);
        scene.append(bottom);
        scene.append(far);
        Image im = Image.createImage(12, 12);
        Graphics g = im.getGraphics();
        g.translate(1, 0);
        g.setClip(0, 0, 11, 12);
        scene.setViewWindow(1, 1, 5, 5);
        scene.paint(g, 2, 3);   // view (1, 1) at (2, 3) translated = (3, 3) on the image
        Check.eq("lm top layer", pixel(im, 4, 4), 0xFFFF0000);
        Check.eq("lm bottom layer", pixel(im, 3, 3), 0xFF0000FF);
        Check.eq("lm outside the view window", pixel(im, 10, 10), 0xFFFFFFFF);
        Check.yes("lm paint restores the graphics", g.getTranslateX() == 1 && g.getTranslateY() == 0
                && g.getClipX() == 0 && g.getClipWidth() == 11);
    }
}
