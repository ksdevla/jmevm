// Garbage collection under load: linked structures, cycles, arrays of
// references, identity hash codes and monitors that must survive objects
// being moved. Single-threaded; output must match a desktop JVM.
import java.util.Hashtable;
import java.util.Vector;

public class TestGc {
	static class Node {
		int value;
		Node next;
		Node other;
		long wide;
		Object payload;

		Node(int value, Node next) {
			this.value = value;
			this.next = next;
			this.wide = (long) value * 1000000007L;
		}
	}

	static Node keep;
	static Object[] table = new Object[64];

	static int sum(Node n) {
		int s = 0;
		for (; n != null; n = n.next) s += n.value + (int) (n.wide % 1000);
		return s;
	}

	public static void main(String[] args) {
		// A long list kept alive while lots of garbage is made around it.
		Node list = null;
		for (int i = 0; i < 2000; i++) {
			list = new Node(i, list);
			new int[100 + i % 50][0].hashCode();
			if (i % 3 == 0) list.payload = "item" + i;
		}
		keep = list;
		p("list " + sum(list));

		// Cycles and cross links
		Node a = new Node(1, null), b = new Node(2, a), c = new Node(3, b);
		a.next = c;
		a.other = b;
		b.other = c;
		c.other = a;
		for (int i = 0; i < 300; i++) new Node(i, new Node(i, null));
		p("cycle " + a.next.next.next.value + " " + a.other.other.other.value + " " + c.next.next.next.value);

		// Identity hash codes and identity survive moving.
		Object[] objs = new Object[50];
		int[] hashes = new int[50];
		for (int i = 0; i < objs.length; i++) {
			objs[i] = new Object();
			hashes[i] = objs[i].hashCode();
			for (int k = 0; k < 20; k++) table[k] = new byte[512];
		}
		System.gc();
		boolean same = true;
		for (int i = 0; i < objs.length; i++) same &= objs[i].hashCode() == hashes[i];
		p("hash " + same);

		// An object locked while garbage is made keeps its monitor.
		Object lock = new Object();
		int counted = 0;
		synchronized (lock) {
			for (int i = 0; i < 500; i++) {
				synchronized (new Object()) {
					counted += new Node(i, null).value & 1;
				}
			}
			System.gc();
			synchronized (lock) {
				counted++;
			}
		}
		p("monitors " + counted);

		// Strings, string tables and collections across collections
		Hashtable h = new Hashtable();
		Vector v = new Vector();
		for (int i = 0; i < 3000; i++) {
			String k = "key" + (i % 500);
			h.put(k, new Integer(i));
			if (i % 7 == 0) v.addElement(new StringBuffer().append(i).append('/').toString());
			if (i % 100 == 0) System.gc();
		}
		int total = 0;
		for (int i = 0; i < 500; i++) total += ((Integer) h.get("key" + i)).intValue();
		p("table " + h.size() + " " + total + " " + v.size() + " " + v.elementAt(100));
		p("intern " + (("key" + 7).intern() == "key7") + " " + (("key" + 7) == "key7"));

		// Arrays of every type, multi-dimensional and reference arrays
		long[][] longs = new long[30][30];
		double[] doubles = new double[1000];
		String[][] names = new String[10][];
		for (int i = 0; i < 30; i++) {
			for (int j = 0; j < 30; j++) longs[i][j] = (long) i << 40 | j;
			if (i < 10) names[i] = new String[] {"n" + i, null, "m" + i};
			doubles[i * 33] = i / 7.0;
			new char[1000].toString();
		}
		long ls = 0;
		for (int i = 0; i < 30; i++) for (int j = 0; j < 30; j++) ls ^= longs[i][j];
		p("arrays " + ls + " " + doubles[33 * 21] + " " + names[9][2] + " " + names[3][1]);

		// Garbage-heavy loop: far more than the heap holds at once.
		int kept = 0;
		for (int i = 0; i < 20000; i++) {
			int[] junk = new int[256];
			junk[i & 255] = i;
			kept += junk[i & 255] & 1;
		}
		p("churn " + kept + " " + sum(keep) + " " + (keep.next.payload == null) + " " + keep.payload);
	}

	static void p(String s) {
		System.out.println(s);
	}
}
