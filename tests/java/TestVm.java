// VM features the kit does not reach: reflection-lite (Class.forName,
// newInstance, getName), interface constants and their initialization,
// array covariance, clone, synchronized methods, wide locals and big
// switches. Single-threaded; output must match a desktop JVM.
public class TestVm {
	public static class Made {
		static String log = "";
		static { log += "M"; }
		public int value = 7;
		public Made() { value += 35; log += "c"; }
	}

	interface Limits {
		int MAX = 10;                       // constant: no initialization needed
		Object HOLDER = new StringBuffer("held");
	}

	static class UsesLimits implements Limits {}

	static class Counter {
		int n;
		synchronized int bump() { return ++n; }
		static synchronized int twice(int x) { return 2 * x; }
	}

	public static void main(String[] args) throws Exception {
		// Class.forName runs static initialization, newInstance the constructor.
		Class c = Class.forName("TestVm$Made");
		String before = Made.log;
		Made m = (Made) c.newInstance();
		p("reflect " + c.getName() + " " + before + " " + m.value + " " + Made.log + " " + (c.isInstance(m)));
		p("names " + new int[0].getClass().getName() + " " + new String[0][0].getClass().getName() + " "
				+ "s".getClass().getName() + " " + Runnable.class.isInterface() + " " + int[].class.isArray());
		p("assignable " + Object.class.isAssignableFrom(String.class) + " " + String.class.isAssignableFrom(Object.class)
				+ " " + Object[].class.isAssignableFrom(String[].class));

		// interface fields
		p("iface " + Limits.MAX + " " + UsesLimits.HOLDER);

		// arrays: covariance, clone, instanceof
		Object[] objs = new String[] {"a", "b"};
		String[] copy = (String[]) objs.clone();
		int[][] grid = {{1, 2}, {3, 4}};
		int[][] shallow = (int[][]) grid.clone();
		shallow[0][0] = 9;
		p("arrays " + (objs instanceof String[]) + " " + (objs instanceof Object[]) + " " + copy[1] + " "
				+ grid[0][0] + " " + (shallow[1] == grid[1]) + " " + ((Object) grid instanceof Object[]));

		// synchronized methods, also re-entered
		Counter k = new Counter();
		int s = 0;
		for (int i = 0; i < 5; i++) s += k.bump();
		synchronized (k) {
			s += k.bump();
		}
		p("sync " + s + " " + Counter.twice(21));

		// many locals: wide loads, stores and iinc
		p("wide " + wide(3));

		// dense and sparse switches
		int t = 0;
		for (int i = -5; i < 300; i += 3) t += dense(i) + sparse(i * 37);
		p("switch " + t);

		// long and double locals and statics
		long acc = 1;
		double dacc = 1;
		for (int i = 1; i <= 20; i++) {
			acc *= i;
			dacc *= i / 2.0;
		}
		total += acc;
		ratio = dacc;
		p("wides " + total + " " + ratio + " " + (acc >>> 3) + " " + (float) dacc);

		// string switch (hashCode + equals)
		p("strswitch " + kind("apple") + kind("pear") + kind("x"));

		// char arithmetic and conversions
		char ch = 'A';
		ch += 2;
		ch++;
		p("chars " + ch + " " + (int) ch + " " + (char) (ch + 1) + " " + Character.toLowerCase(ch));
	}

	static long total = 5;
	static double ratio;

	static String kind(String s) {
		switch (s) {
			case "apple": return "A";
			case "pear": return "P";
			default: return "?";
		}
	}

	static int wide(int seed) {
		int v0 = seed, v1 = v0 + 1, v2 = v1 + 1, v3 = v2 + 1, v4 = v3 + 1, v5 = v4 + 1, v6 = v5 + 1, v7 = v6 + 1;
		int v8 = v7 + 1, v9 = v8 + 1, v10 = v9 + 1, v11 = v10 + 1, v12 = v11 + 1, v13 = v12 + 1, v14 = v13 + 1;
		long w0 = v14, w1 = w0 * 3, w2 = w1 * 3, w3 = w2 * 3, w4 = w3 * 3, w5 = w4 * 3, w6 = w5 * 3, w7 = w6 * 3;
		long w8 = w7 * 3, w9 = w8 * 3, w10 = w9 * 3, w11 = w10 * 3, w12 = w11 * 3, w13 = w12 * 3, w14 = w13 * 3;
		double d0 = w14, d1 = d0 / 2, d2 = d1 / 2, d3 = d2 / 2, d4 = d3 / 2, d5 = d4 / 2, d6 = d5 / 2, d7 = d6 / 2;
		double d8 = d7 / 2, d9 = d8 / 2, d10 = d9 / 2, d11 = d10 / 2, d12 = d11 / 2, d13 = d12 / 2, d14 = d13 / 2;
		long w15 = w14 + 1, w16 = w15 + 1, w17 = w16 + 1, w18 = w17 + 1, w19 = w18 + 1, w20 = w19 + 1;
		double d15 = d14 + 1, d16 = d15 + 1, d17 = d16 + 1, d18 = d17 + 1, d19 = d18 + 1, d20 = d19 + 1;
		int x150 = 0;
		for (int i = 0; i < 1000; i++) x150 += 300;
		Object o200 = "obj";
		return (int) (v0 + v14 + w20 % 1000 + (long) d20 + x150) + o200.hashCode();
	}

	static int dense(int i) {
		switch (i) {
			case 0: return 1;
			case 1: return 2;
			case 2: return 3;
			case 3: return 5;
			case 4: return 8;
			case 5: return 13;
			case 6: return 21;
			case 7: return 34;
			default: return -1;
		}
	}

	static int sparse(int i) {
		switch (i) {
			case -185: return 100;
			case 0: return 1;
			case 37: return 7;
			case 1110: return 11;
			case 5550: return 13;
			case 9990: return 17;
			case 100000: return 19;
			default: return 0;
		}
	}

	static void p(String s) {
		System.out.println(s);
	}
}
