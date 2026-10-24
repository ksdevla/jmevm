package jmevm.test.touch;

import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Display;
import javax.microedition.lcdui.Form;
import javax.microedition.lcdui.Graphics;
import javax.microedition.midlet.MIDlet;

/**
 * Canvas input: touches and keys arrive as the specification says, with the
 * coordinates and key codes the script (keys.txt) sends; game actions;
 * repaints and serviceRepaints; showNotify and hideNotify. Every event is
 * printed, and the screen shows the trail of touches.
 */
public class TouchMIDlet extends MIDlet {
    private Board board;

    protected void startApp() {
        if (board != null) {
            return;
        }
        board = new Board(this);
        Display.getDisplay(this).setCurrent(board);
        new Thread(new Runnable() {
            public void run() {
                board.checks();
            }
        }).start();
    }

    protected void pauseApp() {
    }

    protected void destroyApp(boolean unconditional) {
    }

    static class Board extends Canvas {
        private final MIDlet midlet;
        private final int[] xs = new int[64], ys = new int[64];
        private int points;
        private String last = "touch the screen";
        private volatile int paints;
        private volatile boolean shown;

        Board(MIDlet midlet) {
            this.midlet = midlet;
        }

        void checks() {
            try {
                for (int i = 0; i < 50 && !shown; i++) {
                    Thread.sleep(10);
                }
                Check.yes("showNotify before the canvas is used", shown && isShown());
                Check.yes("pointer events", hasPointerEvents() && hasPointerMotionEvents());
                Check.yes("canvas size", getWidth() > 0 && getHeight() > 0);
                int[] actions = {UP, DOWN, LEFT, RIGHT, FIRE, GAME_A, GAME_B, GAME_C, GAME_D};
                boolean roundTrip = true;
                for (int i = 0; i < actions.length; i++) {
                    roundTrip &= getGameAction(getKeyCode(actions[i])) == actions[i];
                }
                Check.yes("game actions and key codes", roundTrip);
                Check.yes("key names", getKeyName(KEY_NUM5) != null && getKeyName(getKeyCode(FIRE)) != null);
                Check.fails("game action of an invalid key", IllegalArgumentException.class, new Check.Action() {
                    public void run() {
                        getGameAction(0);
                    }
                });

                int before = paints;
                repaint();
                serviceRepaints();
                Check.yes("serviceRepaints paints before it returns", paints > before);
                final boolean[] ran = new boolean[1];
                Display.getDisplay(midlet).callSerially(new Runnable() {
                    public void run() {
                        ran[0] = true;
                    }
                });
                Thread.sleep(100);
                Check.yes("callSerially", ran[0]);

                // Another screen hides the canvas; going back shows it again.
                Form other = new Form("Other");
                Display.getDisplay(midlet).setCurrent(other);
                Thread.sleep(100);
                boolean hidden = !shown && !isShown();
                Display.getDisplay(midlet).setCurrent(this);
                Thread.sleep(100);
                Check.yes("hideNotify and showNotify", hidden && shown);
            } catch (Throwable t) {
                Check.yes("no exception: " + t, false);
            }
            Check.summary();
        }

        protected void showNotify() {
            shown = true;
        }

        protected void hideNotify() {
            shown = false;
        }

        private void event(String what, int x, int y) {
            last = what + " " + x + "," + y;
            System.out.println(last);
            if (points < xs.length) {
                xs[points] = x;
                ys[points] = y;
                points++;
            }
            repaint();
        }

        protected void pointerPressed(int x, int y) {
            event("pressed", x, y);
        }

        protected void pointerDragged(int x, int y) {
            event("dragged", x, y);
        }

        protected void pointerReleased(int x, int y) {
            event("released", x, y);
        }

        protected void keyPressed(int keyCode) {
            last = "key " + keyCode + " pressed, game action " + getGameAction(keyCode);
            System.out.println(last);
            repaint();
        }

        protected void keyReleased(int keyCode) {
            System.out.println("key " + keyCode + " released");
        }

        protected void paint(Graphics g) {
            paints++;
            int w = getWidth(), h = getHeight();
            g.setColor(0x10203A);
            g.fillRect(0, 0, w, h);
            g.setColor(0x305080);
            for (int x = 0; x < w; x += 20) {
                g.drawLine(x, 0, x, h);
            }
            for (int y = 0; y < h; y += 20) {
                g.drawLine(0, y, w, y);
            }
            g.setColor(0xFFC040);
            for (int i = 0; i < points; i++) {
                g.fillArc(xs[i] - 4, ys[i] - 4, 9, 9, 0, 360);
                if (i > 0) {
                    g.drawLine(xs[i - 1], ys[i - 1], xs[i], ys[i]);
                }
            }
            g.setColor(0xFFFFFF);
            g.drawString(last, 4, 4, Graphics.TOP | Graphics.LEFT);
        }
    }
}
