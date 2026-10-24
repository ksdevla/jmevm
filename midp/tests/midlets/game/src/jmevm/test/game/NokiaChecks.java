package jmevm.test.game;

import javax.microedition.lcdui.Command;
import javax.microedition.lcdui.Graphics;
import javax.microedition.lcdui.Image;

import com.nokia.mid.sound.Sound;
import com.nokia.mid.sound.SoundListener;
import com.nokia.mid.ui.DirectGraphics;
import com.nokia.mid.ui.DirectUtils;
import com.nokia.mid.ui.FullCanvas;

/** The Nokia UI API (com.nokia.mid.ui, com.nokia.mid.sound), against Nokia's specification. */
final class NokiaChecks {
    private NokiaChecks() {
    }

    static class Full extends FullCanvas {
        protected void paint(Graphics g) {
        }
    }

    static int pixel(Image im, int x, int y) {
        int[] p = new int[1];
        im.getRGB(p, 0, 1, x, y, 1, 1);
        return p[0];
    }

    static void run() throws InterruptedException {
        final Full full = new Full();
        Check.fails("nokia FullCanvas takes no commands", IllegalStateException.class, new Check.Action() {
            public void run() {
                full.addCommand(new Command("x", Command.OK, 1));
            }
        });
        Check.eq("nokia soft key codes", FullCanvas.KEY_SOFTKEY1 * 10 + FullCanvas.KEY_SOFTKEY2, -67);

        Image filled = DirectUtils.createImage(4, 4, 0xFF00FF00);
        Check.yes("nokia createImage", filled.isMutable() && pixel(filled, 3, 3) == 0xFF00FF00);

        int red = 0xFFFF0000, green = 0xFF00FF00, white = 0xFFFFFFFF;
        Image im = Image.createImage(8, 8);
        DirectGraphics dg = DirectUtils.getDirectGraphics(im.getGraphics());
        dg.drawPixels(new int[] {red, 0x00123456}, true, 0, 2, 1, 1, 2, 1, 0, DirectGraphics.TYPE_INT_8888_ARGB);
        Check.yes("nokia drawPixels with transparency", pixel(im, 1, 1) == red && pixel(im, 2, 1) == white);
        dg.drawPixels(new int[] {red, green}, false, 0, 2, 1, 2, 2, 1, DirectGraphics.FLIP_HORIZONTAL,
                DirectGraphics.TYPE_INT_8888_ARGB);
        Check.yes("nokia drawPixels flipped", pixel(im, 1, 2) == green && pixel(im, 2, 2) == red);
        // ROTATE_90 turns counter-clockwise: the right end of the row comes out on top.
        dg.drawPixels(new int[] {red, green}, false, 0, 2, 5, 1, 2, 1, DirectGraphics.ROTATE_90,
                DirectGraphics.TYPE_INT_8888_ARGB);
        Check.yes("nokia drawPixels rotated 90", pixel(im, 5, 1) == green && pixel(im, 5, 2) == red);
        dg.drawPixels(new short[] {(short) 0xF00F, (short) 0x0FFF}, true, 0, 2, 1, 4, 2, 1, 0,
                DirectGraphics.TYPE_USHORT_4444_ARGB);
        Check.yes("nokia drawPixels 4444", pixel(im, 1, 4) == 0xFF0000FF && pixel(im, 2, 4) == white);
        int[] back = new int[2];
        dg.getPixels(back, 0, 2, 1, 2, 2, 1, DirectGraphics.TYPE_INT_8888_ARGB);
        Check.yes("nokia getPixels", back[0] == green && back[1] == red);
        dg.fillPolygon(new int[] {0, 0, 9, 9}, 0, new int[] {6, 9, 9, 6}, 0, 4, 0xFF112233);
        Check.eq("nokia fillPolygon", pixel(im, 4, 7), 0xFF112233);
        Check.yes("nokia native pixel format", dg.getNativePixelFormat() != 0);

        final StringBuffer events = new StringBuffer();
        Sound beep = new Sound(880, 200);
        Check.eq("nokia sound starts stopped", beep.getState(), Sound.SOUND_STOPPED);
        beep.setSoundListener(new SoundListener() {
            public void soundStateChanged(Sound sound, int event) {
                events.append(' ').append(event);
            }
        });
        beep.play(1);
        Check.eq("nokia sound playing", beep.getState(), Sound.SOUND_PLAYING);
        Thread.sleep(400);
        Check.eq("nokia sound done", beep.getState(), Sound.SOUND_STOPPED);
        Check.eq("nokia sound events", events.toString(), " " + Sound.SOUND_PLAYING + " " + Sound.SOUND_STOPPED);
    }
}
