// jmevm-specific heap behaviour (run with --heap 1): running out of memory
// is an OutOfMemoryError the program can catch and recover from, and weak
// references are cleared by a collection. A desktop JVM with a large heap
// behaves differently, so TestHeapLimits.expected is written by hand.
import java.lang.ref.WeakReference;
import java.util.Vector;

public class TestHeapLimits {
	public static void main(String[] args) {
		Vector hog = new Vector();
		boolean caught = false;
		try {
			while (true) hog.addElement(new int[16 * 1024]);
		} catch (OutOfMemoryError e) {
			caught = true;
		}
		int held = hog.size();
		hog = null;
		int[] after = new int[16 * 1024];
		System.out.println("oom " + caught + " " + (held > 10) + " " + after.length);

		Object strong = new StringBuffer("kept");
		WeakReference weakKept = new WeakReference(strong);
		WeakReference weakLost = new WeakReference(new StringBuffer("lost"));
		System.gc();
		System.out.println("weak " + weakKept.get() + " " + weakLost.get() + " " + (strong != null));

		long free = Runtime.getRuntime().freeMemory();
		long total = Runtime.getRuntime().totalMemory();
		System.out.println("memory " + (free > 0) + " " + (free <= total) + " " + (total > 1000000));
	}
}
