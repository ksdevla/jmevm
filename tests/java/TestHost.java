// The host API, through the natives of jmevm --test-host (tools/testhost.c):
// host state, field access, arrays and strings made by a native, exceptions,
// long and double arguments, properties, resources, blocking on host input
// (also from a second thread) and exit. Expected output written by hand.
import java.io.InputStream;

public class TestHost {
	static class Point {
		int x, y;
		long big;
		Object tag;
	}

	static native int counter();
	static native int nextEvent(int timeoutMs);
	static native int sum(Point p);
	static native void move(Point p, int dx, int dy);
	static native int[] squares(int n);
	static native String[] words(int n);
	static native String shout(String s);
	static native void fail(String msg);
	static native long twice(long v);
	static native double half(double v);
	static native void quit(int code);

	static void p(String s) {
		System.out.println(s);
	}

	public static void main(String[] args) throws Exception {
		p("counter " + counter() + " " + counter());

		Point pt = new Point();
		pt.x = 3;
		pt.y = 4;
		pt.big = 1L << 40 | 5;
		p("fields " + sum(pt));
		move(pt, 10, 20);
		p("moved " + pt.x + " " + pt.y + " " + pt.tag);

		int[] sq = squares(6);
		p("squares " + sq[5] + " " + sq.length);
		String[] w = words(300);
		p("words " + w.length + " " + w[0] + " " + w[299]);
		p("shout " + shout("hello, host"));
		try {
			fail("bad input");
		} catch (IllegalStateException e) {
			p("caught " + e.getMessage());
		}
		p("wide " + twice(1L << 40) + " " + half(5.0));
		p("prop " + System.getProperty("test.host") + " " + System.getProperty("not.there"));

		InputStream in = TestHost.class.getResourceAsStream("/greeting.txt");
		byte[] buf = new byte[64];
		int n = in.read(buf);
		p("resource " + new String(buf, 0, n) + " " + (TestHost.class.getResourceAsStream("/missing") == null));

		// The host posts an event (100, 101, ...) every 7 emulated ms.
		int got = 0;
		for (int i = 0; i < 5; i++) got += nextEvent(1000);
		p("events " + got);
		p("poll " + nextEvent(0));
		p("short wait " + nextEvent(1));

		Thread other = new Thread() {
			public void run() {
				p("other thread got " + nextEvent(-1));
			}
		};
		other.start();
		other.join();
		p("counter " + counter());
		quit(3);
		p("not reached");
	}
}
