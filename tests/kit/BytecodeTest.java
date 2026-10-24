// Edge cases of the bytecode set and the CLDC 1.1 runtime. Every line it
// prints must match a desktop JVM (BytecodeTest.expected).
import java.util.Hashtable;
import java.util.Vector;

public class BytecodeTest {
	public static void main(String[] args) throws Exception {
		ints();
		longs();
		floats();
		doubles();
		conversions();
		finallyBlocks();
		exceptions();
		arrays();
		strings();
		interfaces();
		classInit();
		monitors();
		waitNotify();
		interrupts();
		System.out.println("done");
	}

	static void p(String s) {
		System.out.println(s);
	}

	// --- integers -----------------------------------------------------------

	static int iv(int x) { return x; }

	static void ints() {
		int min = iv(Integer.MIN_VALUE), m1 = iv(-1);
		p("idiv " + (min / m1) + " " + (min % m1) + " " + (7 / -2) + " " + (-7 % 3) + " " + (7 % -3));
		p("ishift " + (1 << iv(33)) + " " + (-1 >> iv(40)) + " " + (-1 >>> iv(31)) + " " + (8 << iv(-1)) + " " + (-16 >> iv(-30)));
		p("iover " + (min - 1) + " " + (Integer.MAX_VALUE + 1) + " " + (min * m1) + " " + (-min));
		byte b = (byte) iv(200);
		short s = (short) iv(70000);
		char c = (char) iv(-1);
		p("narrow " + b + " " + s + " " + (int) c + " " + (byte) (b + b) + " " + (int) (char) (c + 2));
		int x = iv(5);
		x += 1000;
		x -= 70000;
		p("iinc " + x);
		try {
			p("no " + (iv(1) / iv(0)));
		} catch (ArithmeticException e) {
			p("idiv0 " + e.getMessage());
		}
		try {
			p("no " + (iv(1) % iv(0)));
		} catch (ArithmeticException e) {
			p("irem0 caught");
		}
	}

	static long lv(long x) { return x; }

	static void longs() {
		long min = lv(Long.MIN_VALUE);
		p("ldiv " + (min / lv(-1)) + " " + (min % lv(-1)) + " " + (lv(-7) / 2) + " " + (lv(-7) % 2));
		p("lshift " + (lv(1) << iv(65)) + " " + (lv(-1) >>> iv(63)) + " " + (lv(-256) >> iv(-60)) + " " + (lv(1) << iv(63)));
		p("lmul " + (lv(0x123456789L) * lv(0x987654321L)) + " " + (lv(Long.MAX_VALUE) + 1));
		p("lcmp " + (lv(5) < lv(-5)) + " " + (lv(min) < lv(0)) + " " + (lv(3) == lv(3)));
		p("lbits " + (lv(0xF0F0F0F0F0F0F0F0L) & lv(0x0FF00FF00FF00FF0L)) + " " + (lv(-1) ^ lv(0x5555555555555555L)) + " " + (~lv(0)));
		try {
			p("no " + (lv(1) / lv(0)));
		} catch (ArithmeticException e) {
			p("ldiv0 caught");
		}
	}

	// --- floating point -----------------------------------------------------

	static float fv(float x) { return x; }
	static double dv(double x) { return x; }

	static void floats() {
		float nan = fv(Float.NaN), inf = fv(Float.POSITIVE_INFINITY), nz = fv(-0.0f);
		p("fnan " + (nan < 1) + " " + (nan > 1) + " " + (nan == nan) + " " + (nan != nan) + " " + (1 <= nan) + " " + (1 >= nan));
		p("finf " + (inf - inf) + " " + (inf * 0) + " " + (1 / nz) + " " + (-1 / fv(0)) + " " + (nz == 0.0f));
		p("frem " + (fv(5.5f) % 2) + " " + (fv(-5.5f) % 2) + " " + (fv(5.5f) % -2) + " " + (fv(1e10f) % 3) + " " + (inf % 2) + " " + (fv(2) % inf));
		p("fmath " + (fv(0.1f) + fv(0.2f)) + " " + (fv(1) / 3) + " " + (fv(16777216) + 1) + " " + (fv(1e38f) * 10) + " " + fv(Float.MIN_VALUE) / 2);
		p("fstr " + 1.0f + " " + 100.0f + " " + 1e7f + " " + 1.0e-3f + " " + 1.0e-4f + " " + 123456.79f + " " + Float.MAX_VALUE + " " + Float.MIN_VALUE);
		p("fbits " + Integer.toHexString(Float.floatToIntBits(fv(-2.5f))) + " " + Float.intBitsToFloat(0x3f800001) + " " + Integer.toHexString(Float.floatToIntBits(nan)));
	}

	static void doubles() {
		double nan = dv(Double.NaN), inf = dv(Double.POSITIVE_INFINITY);
		p("dnan " + (nan < 1) + " " + (nan > 1) + " " + (nan == nan) + " " + Double.isNaN(nan) + " " + Double.isInfinite(-inf));
		p("drem " + (dv(5.5) % 2) + " " + (dv(-5.5) % 2) + " " + (dv(1e300) % 7) + " " + (dv(-0.0) % 5) + " " + (inf % 1));
		p("dmath " + (dv(0.1) + dv(0.2)) + " " + (dv(1) / 3) + " " + (dv(1e308) * 10) + " " + Math.sqrt(dv(2)) + " " + Math.floor(dv(-1.5)) + " " + Math.ceil(dv(-1.5)));
		p("dstr " + 1.0 + " " + 1e7 + " " + 1.0e-3 + " " + 9.999e-4 + " " + 123456789.0 + " " + Double.MAX_VALUE + " " + Double.MIN_VALUE + " " + (-0.0));
		p("dparse " + Double.parseDouble("1.5e3") + " " + Double.parseDouble("  -2.25  ") + " " + Float.parseFloat("3.4028235E38") + " " + Double.valueOf("0.1"));
		try {
			Double.parseDouble("1.2.3");
			p("no");
		} catch (NumberFormatException e) {
			p("dparse bad caught");
		}
		p("dabs " + Math.abs(dv(-3.5)) + " " + Math.max(dv(-0.0), dv(0.0)) + " " + Math.min(fv(0.0f), fv(-0.0f)) + " " + Math.min(fv(1), fv(-1)) + " " + Math.abs(Integer.MIN_VALUE));
	}

	static void conversions() {
		p("f2i " + (int) fv(1e20f) + " " + (int) fv(-1e20f) + " " + (int) fv(Float.NaN) + " " + (int) fv(-2.9f) + " " + (long) fv(1e20f) + " " + (long) fv(-0.5f));
		p("d2i " + (int) dv(3e9) + " " + (long) dv(-1e19) + " " + (long) dv(Double.NaN) + " " + (int) dv(-0.99) + " " + (char) dv(65.7));
		p("i2f " + (float) iv(16777217) + " " + (double) lv(Long.MAX_VALUE) + " " + (float) lv(Long.MIN_VALUE) + " " + (float) lv(9007199254740993L));
		p("d2f " + (float) dv(1e40) + " " + (float) dv(1e-50) + " " + (float) dv(0.1) + " " + (double) fv(0.1f));
		p("l2i " + (int) lv(0x1234567890L) + " " + (short) lv(-32769) + " " + (byte) lv(128));
	}

	// --- control flow -------------------------------------------------------

	static int depth;

	static int overrideReturn() {
		try {
			return 1;
		} finally {
			return 2;
		}
	}

	static int finallyRuns(int mode) {
		int r = 0;
		for (int i = 0; i < 3; i++) {
			try {
				if (mode == 0 && i == 1) continue;
				if (mode == 1 && i == 1) break;
				if (mode == 2 && i == 2) return r + 100;
				r += 1;
			} finally {
				r += 10;
			}
		}
		return r;
	}

	static String nested() {
		StringBuffer sb = new StringBuffer();
		try {
			try {
				sb.append('a');
				throw new IllegalStateException("x");
			} finally {
				sb.append('b');
				try {
					sb.append('c');
				} finally {
					sb.append('d');
				}
			}
		} catch (IllegalStateException e) {
			sb.append('e').append(e.getMessage());
		} finally {
			sb.append('f');
		}
		return sb.toString();
	}

	static void finallyBlocks() {
		p("finally " + overrideReturn() + " " + finallyRuns(0) + " " + finallyRuns(1) + " " + finallyRuns(2) + " " + nested());
		try {
			try {
				throw new RuntimeException("first");
			} finally {
				if (iv(1) == 1) throw new IllegalArgumentException("second");
			}
		} catch (RuntimeException e) {
			p("replaced " + e.getClass().getName() + " " + e.getMessage());
		}
	}

	static class MyException extends Exception {
		int code;
		MyException(int code) { super("code " + code); this.code = code; }
	}

	static void thrower(int n) throws MyException {
		if (n == 0) throw new MyException(42);
		thrower(n - 1);
	}

	static int recurse(int n) { return n == 0 ? 0 : 1 + recurse(n - 1); }

	static void exceptions() {
		try {
			thrower(50);
		} catch (MyException e) {
			p("deep " + e.code + " " + e.getMessage() + " " + (e instanceof Exception));
		}
		try {
			Object o = new Vector();
			((Hashtable) o).size();
		} catch (ClassCastException e) {
			p("cce caught");
		}
		try {
			Object[] arr = new String[1];
			arr[0] = new Integer(1);
		} catch (ArrayStoreException e) {
			p("ase caught");
		}
		try {
			int[] neg = new int[iv(-1)];
			p("no " + neg.length);
		} catch (NegativeArraySizeException e) {
			p("nase caught");
		}
		try {
			throw null;
		} catch (NullPointerException e) {
			p("throw null caught");
		}
		try {
			Class.forName("does.not.Exist");
		} catch (ClassNotFoundException e) {
			p("cnfe caught");
		}
		p("recurse " + recurse(400));
		Throwable t = new Error("err");
		p("throwable " + t.getMessage() + " " + t.toString() + " " + new RuntimeException().getMessage());
	}

	// --- arrays and strings -------------------------------------------------

	static void arrays() {
		int[][] jag = new int[3][];
		jag[1] = new int[] { 1, 2, 3 };
		long[][][] cube = new long[2][3][4];
		cube[1][2][3] = 99;
		p("arrays " + (jag[0] == null) + " " + jag[1][2] + " " + cube[1][2][3] + " " + cube[1].length + " " + cube[0][0].length);
		int[] a = { 1, 2, 3, 4, 5, 6 };
		System.arraycopy(a, 0, a, 2, 4);
		p("overlap " + a[0] + a[1] + a[2] + a[3] + a[4] + a[5]);
		int[] c = (int[]) a.clone();
		c[0] = 9;
		p("clone " + a[0] + " " + c[0] + " " + (c != a) + " " + c.length);
		Object[] objs = new Object[2];
		String[] strs = { "x", "y" };
		System.arraycopy(strs, 0, objs, 0, 2);
		p("copyref " + objs[1]);
		boolean[] bools = new boolean[2];
		bools[1] = true;
		char[] chars = new char[] { 'h', 'i' };
		p("prim " + bools[0] + bools[1] + " " + new String(chars) + " " + new byte[3].length);
		try {
			System.arraycopy(a, 4, a, 0, 3);
			p("no");
		} catch (ArrayIndexOutOfBoundsException e) {
			p("arraycopy bounds caught");
		}
	}

	static void strings() {
		String a = "hello";
		String b = new String("hel") + "lo";
		p("intern " + (a == "hello") + " " + (a == b) + " " + (a == b.intern()) + " " + a.equals(b) + " " + (a.hashCode() == b.hashCode()));
		StringBuffer sb = new StringBuffer("abcdef");
		sb.insert(2, "XY").reverse().deleteCharAt(0).setCharAt(0, 'Q');
		p("sb " + sb + " " + sb.length());
		p("str " + "a,b,,c".indexOf(",,") + " " + "Hello".toUpperCase() + " " + "  t  ".trim() + "|" + " " + "abc".compareTo("abd") + " " + "x".concat("y") + " " + "banana".lastIndexOf('a') + " " + "abc".endsWith("bc"));
		p("valueOf " + String.valueOf(true) + " " + String.valueOf('c') + " " + String.valueOf(12L) + " " + String.valueOf(1.5f) + " " + Integer.parseInt("-123") + " " + Integer.toString(255, 16) + " " + Long.parseLong("9000000000"));
		char ch = 'q';
		switch (ch) {
			case 'a': p("switch a"); break;
			case 'q': p("switch q"); break;
			default: p("switch default");
		}
		p("chars " + Character.isDigit('7') + " " + Character.toUpperCase('z') + " " + Character.isUpperCase('A') + " " + (int) Character.toLowerCase('Q'));
	}

	// --- interfaces and virtual dispatch -------------------------------------

	interface Shape {
		int area();
	}

	static abstract class Base implements Shape {
		public String toString() { return "Base" + area(); }
		int twice() { return 2 * area(); }
	}

	static class Sq extends Base {
		private final int s;
		Sq(int s) { this.s = s; }
		public int area() { return s * s; }
		public boolean equals(Object o) { return o instanceof Sq && ((Sq) o).s == s; }
		public int hashCode() { return s; }
	}

	static void interfaces() {
		Shape sh = new Sq(3);
		Runnable r = new Runnable() {
			public void run() {}
			public String toString() { return "runnable"; }
		};
		// Object methods called through interface types
		p("iface " + sh.area() + " " + sh.toString() + " " + sh.hashCode() + " " + sh.equals(new Sq(3)) + " " + r.toString());
		p("abstract " + ((Base) sh).twice() + " " + (sh instanceof Base) + " " + (r instanceof Shape));
		Hashtable h = new Hashtable();
		h.put(new Sq(2), "two");
		p("hashkey " + h.get(new Sq(2)) + " " + h.containsKey(new Sq(3)));
	}

	// --- class initialization ----------------------------------------------

	static StringBuffer initLog = new StringBuffer();

	static class Parent {
		static { initLog.append("P"); }
		static int pv = 1;
	}

	static class Child extends Parent {
		static { initLog.append("C"); }
		static final int CONST = 7;   // compile-time constant: no init needed
		static int cv = pv + 1;
	}

	static class Faulty {
		static int value = 1 / iv(0);
	}

	static void classInit() {
		int k = Child.CONST;
		String before = initLog.toString();
		int v = Child.cv;
		p("clinit " + k + " [" + before + "] " + v + " " + initLog);
		try {
			p("no " + Faulty.value);
		} catch (Throwable e) {
			p("clinit failed once");
		}
		try {
			p("no " + Faulty.value);
		} catch (Throwable e) {
			p("clinit failed twice");
		}
	}

	// --- threads ------------------------------------------------------------

	static int shared;

	static synchronized void bump() { shared++; }

	synchronized int reenter(int n) { return n == 0 ? 0 : 1 + reenter(n - 1); }

	static void monitors() throws Exception {
		Thread[] ts = new Thread[3];
		for (int i = 0; i < 3; i++) {
			ts[i] = new Thread() {
				public void run() { for (int j = 0; j < 500; j++) bump(); }
			};
			ts[i].start();
		}
		for (int i = 0; i < 3; i++) ts[i].join();
		p("synchronized " + shared + " " + new BytecodeTest().reenter(20));
		Object lock = new Object();
		try {
			lock.notify();
			p("no");
		} catch (IllegalMonitorStateException e) {
			p("notify unowned caught");
		}
		try {
			lock.wait();
			p("no");
		} catch (IllegalMonitorStateException e) {
			p("wait unowned caught");
		}
		// An exception leaving a synchronized method releases its lock.
		final Object m = new Object();
		try {
			synchronized (m) {
				throw new RuntimeException("inside");
			}
		} catch (RuntimeException e) {
			Thread other = new Thread() {
				public void run() { synchronized (m) { shared = -1; } }
			};
			other.start();
			other.join();
			p("released " + shared);
		}
	}

	static final Object queueLock = new Object();
	static Vector queue = new Vector();

	static void waitNotify() throws Exception {
		Thread consumer = new Thread() {
			public void run() {
				int got = 0;
				synchronized (queueLock) {
					while (got < 5) {
						while (queue.isEmpty()) {
							try { queueLock.wait(); } catch (InterruptedException e) { return; }
						}
						got += ((Integer) queue.elementAt(0)).intValue();
						queue.removeElementAt(0);
					}
				}
				p("consumer " + got);
			}
		};
		consumer.start();
		for (int i = 0; i < 5; i++) {
			synchronized (queueLock) {
				queue.addElement(new Integer(1));
				queueLock.notify();
			}
			Thread.yield();
		}
		consumer.join();
		long t0 = System.currentTimeMillis();
		synchronized (queueLock) { queueLock.wait(30); }
		long waited = System.currentTimeMillis() - t0;
		p("timed wait " + (waited >= 30) + " " + consumer.isAlive());
	}

	static void interrupts() throws Exception {
		Thread sleeper = new Thread() {
			public void run() {
				try {
					Thread.sleep(100000);
					p("sleeper woke");
				} catch (InterruptedException e) {
					p("sleeper interrupted");
				}
			}
		};
		sleeper.start();
		Thread.sleep(10);
		sleeper.interrupt();
		sleeper.join();
		final Object o = new Object();
		Thread waiter = new Thread() {
			public void run() {
				synchronized (o) {
					try {
						o.wait();
						p("waiter woke");
					} catch (InterruptedException e) {
						p("waiter interrupted");
					}
				}
			}
		};
		waiter.start();
		Thread.sleep(10);
		waiter.interrupt();
		waiter.join();
		p("threads alive " + sleeper.isAlive() + " " + waiter.isAlive() + " " + Thread.currentThread().getName());
	}
}
