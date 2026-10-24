// Small VM conformance test limited to CLDC 1.1 classes.
// Every line printed is compared against the output of a desktop JVM.
import java.util.Hashtable;
import java.util.Vector;

public class VmTest {
	interface Shape { int area(); }
	static class Rect implements Shape {
		int w, h;
		Rect(int w, int h) { this.w = w; this.h = h; }
		public int area() { return w * h; }
	}
	static class Square extends Rect {
		Square(int s) { super(s, s); }
		public int area() { return super.area() + 1; }
	}

	static int counter;
	static final Object lock = new Object();
	static String staticInit = "init:" + (7 * 6);

	public static void main(String[] args) throws Exception {
		System.out.println("start " + staticInit);

		// integer and long arithmetic, overflow, shifts, division
		int a = 0x7FFFFFFF;
		a++;
		System.out.println("int " + a + " " + (-7 / 2) + " " + (-7 % 2) + " " + (-8 >> 1) + " " + (-8 >>> 28));
		long l = 0x123456789ABCDEFL;
		l = l * 31 + (l >>> 7) ^ (l << 3);
		System.out.println("long " + l + " " + (l / 1000) + " " + (l % 977));

		// float and double
		float f = 1.0f / 3;
		double d = Math.sqrt(2.0) * 1e10;
		System.out.println("float " + f + " " + (int) (f * 1e6f) + " double " + d + " " + (long) d);
		System.out.println("conv " + (int) 3.99 + " " + (int) -3.99 + " " + (int) Float.NaN + " " + (long) 1e30 + " " + (char) 65);

		// strings and StringBuffer
		StringBuffer sb = new StringBuffer();
		for (int i = 0; i < 10; i++) sb.append(i).append(',');
		String s = sb.toString();
		System.out.println("str " + s + " " + s.length() + " " + s.indexOf("5") + " " + s.substring(4, 9) + " " + s.hashCode());

		// arrays, multi-dimensional arrays, System.arraycopy
		int[][] grid = new int[4][5];
		for (int y = 0; y < 4; y++) for (int x = 0; x < 5; x++) grid[y][x] = y * 10 + x;
		int[] copy = new int[5];
		System.arraycopy(grid[3], 1, copy, 0, 4);
		int sum = 0;
		for (int i = 0; i < copy.length; i++) sum += copy[i];
		byte[] bytes = new byte[] { (byte) 200, 100, -1 };
		System.out.println("arr " + sum + " " + bytes[0] + " " + (bytes[0] & 0xFF) + " " + grid.length);

		// virtual calls, interfaces, instanceof, casts
		Shape[] shapes = { new Rect(3, 4), new Square(5) };
		int total = 0;
		for (int i = 0; i < shapes.length; i++) total += shapes[i].area();
		System.out.println("oop " + total + " " + (shapes[1] instanceof Rect) + " " + (shapes[0] instanceof Square));

		// exceptions, finally, nested try
		System.out.println("exc " + tryFinally(0) + " " + tryFinally(1) + " " + tryFinally(2));
		try {
			Object o = "x";
			Integer bad = (Integer) o;
			System.out.println("no " + bad);
		} catch (ClassCastException e) {
			System.out.println("cce caught");
		}
		try {
			int[] small = new int[2];
			small[5] = 1;
		} catch (ArrayIndexOutOfBoundsException e) {
			System.out.println("aioobe caught");
		}
		try {
			Object o = null;
			o.hashCode();
		} catch (NullPointerException e) {
			System.out.println("npe caught");
		}

		// switch statements
		int sw = 0;
		for (int i = -2; i < 12; i++) sw += tableSwitch(i) * 3 + lookupSwitch(i * 100);
		System.out.println("switch " + sw);

		// CLDC collections
		Hashtable h = new Hashtable();
		Vector v = new Vector();
		for (int i = 0; i < 50; i++) {
			h.put("k" + i, new Integer(i * i));
			v.addElement(new Integer(i));
		}
		System.out.println("coll " + h.get("k7") + " " + h.size() + " " + v.elementAt(49) + " " + v.size());

		// threads, synchronized, wait/notify, sleep
		Thread[] ts = new Thread[4];
		for (int i = 0; i < ts.length; i++) {
			ts[i] = new Thread(new Runnable() {
				public void run() {
					for (int k = 0; k < 1000; k++) synchronized (lock) { counter++; }
				}
			});
			ts[i].start();
		}
		for (int i = 0; i < ts.length; i++) ts[i].join();
		System.out.println("threads " + counter);

		final int[] box = new int[1];
		Thread waiter = new Thread() {
			public void run() {
				synchronized (lock) {
					while (box[0] == 0) {
						try { lock.wait(); } catch (InterruptedException e) { return; }
					}
					box[0]++;
				}
			}
		};
		waiter.start();
		Thread.sleep(20);
		synchronized (lock) { box[0] = 10; lock.notifyAll(); }
		waiter.join();
		System.out.println("wait " + box[0]);

		// garbage: allocate enough to force collections
		int kept = 0;
		for (int i = 0; i < 20000; i++) {
			int[] junk = new int[256];
			junk[i & 255] = i;
			kept += junk[i & 255] & 1;
		}
		System.out.println("gc " + kept + " " + (Runtime.getRuntime().totalMemory() > 0));
		System.out.println("done");
	}

	static int tryFinally(int mode) {
		int r = 0;
		try {
			try {
				if (mode == 1) throw new RuntimeException("x");
				if (mode == 2) return 5;
				r = 1;
			} finally {
				r += 10;
			}
		} catch (RuntimeException e) {
			r += 100;
		}
		return r;
	}

	static int tableSwitch(int i) {
		switch (i) {
			case 0: return 3;
			case 1: return 5;
			case 2: return 7;
			case 3: case 4: return 9;
			default: return -1;
		}
	}

	static int lookupSwitch(int i) {
		switch (i) {
			case 100: return 1;
			case 5000: return 2;
			case -200: return 4;
			default: return 0;
		}
	}
}
