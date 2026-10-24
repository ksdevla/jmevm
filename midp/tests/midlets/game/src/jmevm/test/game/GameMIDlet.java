package jmevm.test.game;

import javax.microedition.lcdui.Display;
import javax.microedition.lcdui.Graphics;
import javax.microedition.lcdui.Image;
import javax.microedition.lcdui.game.GameCanvas;
import javax.microedition.lcdui.game.LayerManager;
import javax.microedition.lcdui.game.Sprite;
import javax.microedition.lcdui.game.TiledLayer;
import javax.microedition.midlet.MIDlet;
import javax.microedition.rms.RecordStore;

/**
 * Record stores, the game API, resources and connections, and Nokia UI,
 * each checked against its specification. Then a small game: a player
 * sprite on a tiled map that the keys move, coins to collect, and the
 * score saved in a record store whenever fire is pressed.
 */
public class GameMIDlet extends MIDlet {
    protected void startApp() {
        try {
            System.out.println("run " + RmsChecks.countRun());
            RmsChecks.run();
            GameApiChecks.run();
            IoChecks.run();
            NokiaChecks.run();
        } catch (Throwable t) {
            Check.yes("no exception: " + t, false);
        }
        Check.summary();
        Game game = new Game();
        Display.getDisplay(this).setCurrent(game);
        new Thread(game).start();
    }

    protected void pauseApp() {
    }

    protected void destroyApp(boolean unconditional) {
    }

    static class Game extends GameCanvas implements Runnable {
        private final LayerManager layers = new LayerManager();
        private final TiledLayer map;
        private final Sprite player;
        private final Sprite[] coins = new Sprite[4];
        private int score, frame;

        Game() {
            super(true);
            // Tiles 8x8: 1 grass, 2 stone, 3 and 4 water (animated).
            int[] p = new int[32 * 8];
            int[] colours = {0xFF2E7D32, 0xFF757575, 0xFF1565C0, 0xFF1E88E5};
            for (int y = 0; y < 8; y++) {
                for (int x = 0; x < 32; x++) {
                    p[y * 32 + x] = colours[x / 8] ^ ((x + y) % 4 == 0 ? 0x00101010 : 0);
                }
            }
            map = new TiledLayer(30, 40, Image.createRGBImage(p, 32, 8, false), 8, 8);
            map.fillCells(0, 0, 30, 40, 1);
            int water = map.createAnimatedTile(3);
            map.fillCells(10, 12, 8, 5, water);
            for (int i = 0; i < 30; i++) {
                map.setCell(i, 0, 2);
                map.setCell(i, 39, 2);
            }

            int[] q = new int[16 * 8];   // two 8x8 frames
            for (int i = 0; i < q.length; i++) {
                int x = i % 16 % 8, y = i / 16;
                boolean inside = (x - 3.5) * (x - 3.5) + (y - 3.5) * (y - 3.5) < 13;
                q[i] = inside ? (i % 16 < 8 ? 0xFFFFC107 : 0xFFFF9800) : 0;
            }
            player = new Sprite(Image.createRGBImage(q, 16, 8, true), 8, 8);
            player.defineReferencePixel(4, 4);
            player.setRefPixelPosition(60, 60);
            for (int i = 0; i < coins.length; i++) {
                coins[i] = new Sprite(Image.createRGBImage(q, 16, 8, true), 8, 8);
                coins[i].setFrame(1);
                coins[i].setPosition(40 + i * 30, 56 + (i % 2) * 40);
                layers.append(coins[i]);
            }
            layers.insert(player, 0);
            layers.append(map);
        }

        public void run() {
            Graphics g = getGraphics();
            boolean fireWasDown = false;
            for (;;) {
                int keys = getKeyStates();
                int dx = 0, dy = 0;
                if ((keys & UP_PRESSED) != 0) dy = -3;
                if ((keys & DOWN_PRESSED) != 0) dy = 3;
                if ((keys & LEFT_PRESSED) != 0) dx = -3;
                if ((keys & RIGHT_PRESSED) != 0) dx = 3;
                if (dx == 0 && dy == 0) {
                    dx = 2;   // without keys the player wanders to the right
                }
                player.move(dx, dy);
                if (player.collidesWith(map, false) && player.getY() < 8) {
                    player.move(0, -dy);
                }
                player.nextFrame();
                for (int i = 0; i < coins.length; i++) {
                    if (coins[i].isVisible() && player.collidesWith(coins[i], true)) {
                        coins[i].setVisible(false);
                        score += 10;
                        System.out.println("coin " + i + " in frame " + frame + ", score " + score);
                    }
                }
                map.setAnimatedTile(-1, 3 + (frame / 8) % 2);
                boolean fire = (keys & FIRE_PRESSED) != 0;
                if (fire && !fireWasDown) {
                    save();
                }
                fireWasDown = fire;

                int w = getWidth(), h = getHeight();
                int vx = Math.max(0, Math.min(player.getX() - w / 2, map.getWidth() - w));
                int vy = Math.max(0, Math.min(player.getY() - h / 2, map.getHeight() - h));
                layers.setViewWindow(vx, vy, w, h);
                g.setColor(0);
                g.fillRect(0, 0, w, h);
                layers.paint(g, 0, 0);
                g.setColor(0xFFFFFF);
                g.drawString("score " + score, 2, 2, Graphics.TOP | Graphics.LEFT);
                flushGraphics();
                frame++;
                try {
                    Thread.sleep(50);
                } catch (InterruptedException e) {
                    return;
                }
            }
        }

        private void save() {
            try {
                RecordStore rs = RecordStore.openRecordStore("score", true);
                byte[] b = {(byte) (score >> 8), (byte) score};
                if (rs.getNumRecords() == 0) {
                    rs.addRecord(b, 0, 2);
                } else {
                    rs.setRecord(1, b, 0, 2);
                }
                System.out.println("saved score " + score + " in frame " + frame + ", store version "
                        + rs.getVersion());
                rs.closeRecordStore();
            } catch (Exception e) {
                System.out.println("cannot save: " + e);
            }
        }
    }
}
