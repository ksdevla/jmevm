package javax.microedition.lcdui;

public abstract class Canvas extends Displayable {
	static final int W = 240, H = 320;
	public static int framesToRun = 20;
	private static final int[] fb = new int[W * H];
	private static int frames;

	protected Canvas() {}
	protected abstract void paint(Graphics g);
	public final void repaint() {}
	public final void repaint(int x, int y, int w, int h) {}

	public final void serviceRepaints() {
		paint(new Graphics(fb, W, H));
		int h = 0;
		for (int i = 0; i < fb.length; i++) h = h * 31 + fb[i];
		frames++;
		System.out.println("frame " + frames + " " + Integer.toHexString(h));
		if (frames >= framesToRun) {
			System.out.println("done");
			System.exit(0);
		}
	}
}
