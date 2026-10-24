import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Display;
import javax.microedition.lcdui.Graphics;
import javax.microedition.midlet.MIDlet;

public class TestMidlet extends MIDlet {
	private GameCanvas canvas;

	public void startApp() {
		if (canvas == null) {
			canvas = new GameCanvas();
			new Thread(canvas).start();
		}
		Display.getDisplay(this).setCurrent(canvas);
	}

	public void pauseApp() {
	}

	public void destroyApp(boolean unconditional) {
		if (canvas != null) canvas.running = false;
	}

	static class GameCanvas extends Canvas implements Runnable {
		volatile boolean running = true;
		int frame;

		protected void paint(Graphics g) {
			int w = getWidth(), h = getHeight();
			// top half: stripes moving 6 px a frame (trackable)
			int off = (frame * 6) % 32;
			for (int x = -32; x < w + 32; x += 32) {
				g.setColor(0x2050C0);
				g.fillRect(x + off, 0, 16, h / 2);
				g.setColor(0xE0E0E0);
				g.fillRect(x + off + 16, 0, 16, h / 2);
			}
			// bottom half: stripes moving 24 px a frame (faster than the motion search)
			off = (frame * 24) % 40;
			for (int x = -40; x < w + 40; x += 40) {
				g.setColor(0x30A030);
				g.fillRect(x + off, h / 2, 20, h / 2);
				g.setColor(0x202020);
				g.fillRect(x + off + 20, h / 2, 20, h / 2);
			}
			g.setColor(0xFF4040);
			g.fillRect((frame * 3) % (w - 40), h / 2 - 20, 40, 40);
		}

		public void run() {
			while (running) {
				frame++;
				repaint();
				serviceRepaints();
				try {
					Thread.sleep(50);
				} catch (InterruptedException e) {
					return;
				}
			}
		}
	}
}
