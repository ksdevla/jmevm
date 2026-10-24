package javax.microedition.lcdui.game;

import javax.microedition.lcdui.Graphics;
import javax.microedition.lcdui.Image;

/**
 * A layer showing one frame of an image cut into equal frames, played in a
 * frame sequence, with a transform, a reference pixel and a collision
 * rectangle.
 *
 * <p>Coordinates inside a frame are untransformed; a transform maps frame
 * pixels to the drawn (transformed) frame, and the sprite's position is its
 * drawn frame's top left. Setting a transform keeps the reference pixel
 * where it was on screen.
 */
public class Sprite extends Layer {
    public static final int TRANS_NONE = 0;
    public static final int TRANS_MIRROR_ROT180 = 1;
    public static final int TRANS_MIRROR = 2;
    public static final int TRANS_ROT180 = 3;
    public static final int TRANS_MIRROR_ROT270 = 4;
    public static final int TRANS_ROT90 = 5;
    public static final int TRANS_ROT270 = 6;
    public static final int TRANS_MIRROR_ROT90 = 7;

    private Image image;
    private int frameW, frameH, columns, rawFrames;
    private int[] sequence;
    private boolean customSequence;
    private int index;
    private int refX, refY;
    private int transform;
    private int collX, collY, collW, collH;

    public Sprite(Image image) {
        this(image, image.getWidth(), image.getHeight());
    }

    public Sprite(Image image, int frameWidth, int frameHeight) {
        super(frameWidth, frameHeight);
        setFrames(image, frameWidth, frameHeight);
        sequence = defaultSequence();
        collW = frameWidth;
        collH = frameHeight;
    }

    public Sprite(Sprite s) {
        super(s.width, s.height);
        image = s.image;
        frameW = s.frameW;
        frameH = s.frameH;
        columns = s.columns;
        rawFrames = s.rawFrames;
        sequence = (int[]) s.sequence.clone();
        customSequence = s.customSequence;
        index = s.index;
        refX = s.refX;
        refY = s.refY;
        transform = s.transform;
        collX = s.collX;
        collY = s.collY;
        collW = s.collW;
        collH = s.collH;
        x = s.x;
        y = s.y;
        visible = s.visible;
    }

    private void setFrames(Image img, int fw, int fh) {
        if (img == null) {
            throw new NullPointerException();
        }
        if (fw <= 0 || fh <= 0 || img.getWidth() % fw != 0 || img.getHeight() % fh != 0) {
            throw new IllegalArgumentException("frames of " + fw + "x" + fh + " do not divide the image");
        }
        image = img;
        frameW = fw;
        frameH = fh;
        columns = img.getWidth() / fw;
        rawFrames = columns * (img.getHeight() / fh);
    }

    private int[] defaultSequence() {
        int[] s = new int[rawFrames];
        for (int i = 0; i < s.length; i++) {
            s[i] = i;
        }
        return s;
    }

    // ---- frames

    public int getRawFrameCount() {
        return rawFrames;
    }

    public int getFrameSequenceLength() {
        return sequence.length;
    }

    public final int getFrame() {
        return index;
    }

    public void setFrame(int sequenceIndex) {
        if (sequenceIndex < 0 || sequenceIndex >= sequence.length) {
            throw new IndexOutOfBoundsException("frame " + sequenceIndex);
        }
        index = sequenceIndex;
    }

    public void nextFrame() {
        index = (index + 1) % sequence.length;
    }

    public void prevFrame() {
        index = (index + sequence.length - 1) % sequence.length;
    }

    public void setFrameSequence(int[] seq) {
        if (seq == null) {
            sequence = defaultSequence();
            customSequence = false;
            index = 0;
            return;
        }
        if (seq.length < 1) {
            throw new IllegalArgumentException("empty frame sequence");
        }
        for (int i = 0; i < seq.length; i++) {
            if (seq[i] < 0 || seq[i] >= rawFrames) {
                throw new ArrayIndexOutOfBoundsException("frame " + seq[i]);
            }
        }
        sequence = (int[]) seq.clone();
        customSequence = true;
        index = 0;
    }

    public void setImage(Image img, int frameWidth, int frameHeight) {
        int refScreenX = getRefPixelX(), refScreenY = getRefPixelY();
        boolean resized = frameWidth != frameW || frameHeight != frameH;
        setFrames(img, frameWidth, frameHeight);
        if (!customSequence || maxFrame() >= rawFrames) {
            sequence = defaultSequence();
            customSequence = false;
            index = 0;
        }
        if (resized) {
            collX = collY = 0;
            collW = frameWidth;
            collH = frameHeight;
        }
        updateSize();
        setRefPixelPosition(refScreenX, refScreenY);
    }

    private int maxFrame() {
        int m = 0;
        for (int i = 0; i < sequence.length; i++) {
            m = Math.max(m, sequence[i]);
        }
        return m;
    }

    // ---- transform and reference pixel

    private boolean turned() {
        return transform >= TRANS_MIRROR_ROT270;   /* 4..7 swap width and height */
    }

    private void updateSize() {
        width = turned() ? frameH : frameW;
        height = turned() ? frameW : frameH;
    }

    /** Where frame pixel (fx, fy) is drawn in the transformed frame. */
    private int mapX(int fx, int fy) {
        switch (transform) {
        case TRANS_MIRROR: case TRANS_ROT180: return frameW - 1 - fx;
        case TRANS_ROT90: case TRANS_MIRROR_ROT90: return frameH - 1 - fy;
        case TRANS_ROT270: case TRANS_MIRROR_ROT270: return fy;
        default: return fx;   /* TRANS_NONE, TRANS_MIRROR_ROT180 */
        }
    }

    private int mapY(int fx, int fy) {
        switch (transform) {
        case TRANS_MIRROR_ROT180: case TRANS_ROT180: return frameH - 1 - fy;
        case TRANS_ROT90: case TRANS_MIRROR_ROT270: return fx;
        case TRANS_ROT270: case TRANS_MIRROR_ROT90: return frameW - 1 - fx;
        default: return fy;   /* TRANS_NONE, TRANS_MIRROR */
        }
    }

    /** The frame pixel shown at (dx, dy) of the transformed frame, as {fx, fy}. */
    private void unmap(int dx, int dy, int[] out) {
        int w = frameW, h = frameH;
        switch (transform) {
        case TRANS_MIRROR: out[0] = w - 1 - dx; out[1] = dy; break;
        case TRANS_MIRROR_ROT180: out[0] = dx; out[1] = h - 1 - dy; break;
        case TRANS_ROT180: out[0] = w - 1 - dx; out[1] = h - 1 - dy; break;
        case TRANS_ROT90: out[0] = dy; out[1] = h - 1 - dx; break;
        case TRANS_ROT270: out[0] = w - 1 - dy; out[1] = dx; break;
        case TRANS_MIRROR_ROT90: out[0] = w - 1 - dy; out[1] = h - 1 - dx; break;
        case TRANS_MIRROR_ROT270: out[0] = dy; out[1] = dx; break;
        default: out[0] = dx; out[1] = dy; break;
        }
    }

    public void defineReferencePixel(int inp_x, int inp_y) {
        refX = inp_x;
        refY = inp_y;
    }

    public int getRefPixelX() {
        return x + mapX(refX, refY);
    }

    public int getRefPixelY() {
        return y + mapY(refX, refY);
    }

    public void setRefPixelPosition(int inp_x, int inp_y) {
        x = inp_x - mapX(refX, refY);
        y = inp_y - mapY(refX, refY);
    }

    public void setTransform(int t) {
        if (t < 0 || t > 7) {
            throw new IllegalArgumentException("transform " + t);
        }
        int rx = getRefPixelX(), ry = getRefPixelY();
        transform = t;
        updateSize();
        setRefPixelPosition(rx, ry);
    }

    // ---- painting

    public final void paint(Graphics g) {
        if (!visible) {
            return;
        }
        int frame = sequence[index];
        g.drawRegion(image, (frame % columns) * frameW, (frame / columns) * frameH, frameW, frameH, transform, x, y,
                Graphics.TOP | Graphics.LEFT);
    }

    // ---- collisions

    public void defineCollisionRectangle(int cx, int cy, int cw, int ch) {
        if (cw < 0 || ch < 0) {
            throw new IllegalArgumentException("collision rectangle " + cw + "x" + ch);
        }
        collX = cx;
        collY = cy;
        collW = cw;
        collH = ch;
    }

    /** The collision rectangle on screen, as {x1, y1, x2, y2} (exclusive); empty if w or h is 0. */
    private int[] collisionBox() {
        if (collW == 0 || collH == 0) {
            return new int[] {0, 0, 0, 0};
        }
        int ax = mapX(collX, collY), ay = mapY(collX, collY);
        int bx = mapX(collX + collW - 1, collY + collH - 1), by = mapY(collX + collW - 1, collY + collH - 1);
        return new int[] {x + Math.min(ax, bx), y + Math.min(ay, by), x + Math.max(ax, bx) + 1,
                y + Math.max(ay, by) + 1};
    }

    /** Whether the drawn sprite is opaque at screen pixel (sx, sy). */
    private boolean opaqueAt(int sx, int sy, int[] tmp) {
        unmap(sx - x, sy - y, tmp);
        if (tmp[0] < 0 || tmp[1] < 0 || tmp[0] >= frameW || tmp[1] >= frameH) {
            return false;
        }
        int frame = sequence[index];
        return Pixels.opaque(image, (frame % columns) * frameW + tmp[0], (frame / columns) * frameH + tmp[1]);
    }

    private static int[] intersect(int[] a, int[] b) {
        int[] r = {Math.max(a[0], b[0]), Math.max(a[1], b[1]), Math.min(a[2], b[2]), Math.min(a[3], b[3])};
        return r[0] < r[2] && r[1] < r[3] ? r : null;
    }

    public final boolean collidesWith(Sprite s, boolean pixelLevel) {
        if (!visible || !s.visible) {
            return false;
        }
        int[] r = intersect(collisionBox(), s.collisionBox());
        if (r == null || !pixelLevel) {
            return r != null;
        }
        int[] tmp = new int[2];
        for (int py = r[1]; py < r[3]; py++) {
            for (int px = r[0]; px < r[2]; px++) {
                if (opaqueAt(px, py, tmp) && s.opaqueAt(px, py, tmp)) {
                    return true;
                }
            }
        }
        return false;
    }

    public final boolean collidesWith(TiledLayer t, boolean pixelLevel) {
        if (!visible || !t.visible) {
            return false;
        }
        int[] r = intersect(collisionBox(), new int[] {t.x, t.y, t.x + t.width, t.y + t.height});
        if (r == null) {
            return false;
        }
        int[] tmp = new int[2];
        for (int py = r[1]; py < r[3]; py++) {
            for (int px = r[0]; px < r[2]; px++) {
                if (pixelLevel ? t.opaqueAt(px, py) && opaqueAt(px, py, tmp) : t.cellAt(px, py) != 0) {
                    return true;
                }
            }
        }
        return false;
    }

    public final boolean collidesWith(Image img, int ix, int iy, boolean pixelLevel) {
        if (!visible) {
            return false;
        }
        int[] r = intersect(collisionBox(), new int[] {ix, iy, ix + img.getWidth(), iy + img.getHeight()});
        if (r == null || !pixelLevel) {
            return r != null;
        }
        int[] tmp = new int[2];
        for (int py = r[1]; py < r[3]; py++) {
            for (int px = r[0]; px < r[2]; px++) {
                if (opaqueAt(px, py, tmp) && Pixels.opaque(img, px - ix, py - iy)) {
                    return true;
                }
            }
        }
        return false;
    }
}
