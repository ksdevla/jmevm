package javax.microedition.lcdui;

public class Graphics {
	private final int[] fb;
	private final int w, h;
	private int color = 0xFF000000;

	Graphics(int[] fb, int w, int h) { this.fb = fb; this.w = w; this.h = h; }
	public void setColor(int rgb) { color = 0xFF000000 | rgb; }
	public void setColor(int r, int g, int b) { setColor((r << 16) | (g << 8) | b); }
	public int getColor() { return color & 0xFFFFFF; }

	public void fillRect(int x, int y, int rw, int rh) {
		int x0 = Math.max(x, 0), y0 = Math.max(y, 0);
		int x1 = Math.min(x + rw, w), y1 = Math.min(y + rh, h);
		for (int yy = y0; yy < y1; yy++) {
			int row = yy * w;
			for (int xx = x0; xx < x1; xx++) fb[row + xx] = color;
		}
	}
}
