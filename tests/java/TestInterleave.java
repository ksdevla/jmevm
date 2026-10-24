// Threads that race on purpose: unsynchronized updates, yields, sleeps and
// time slices. On a desktop JVM the output changes from run to run; on jmevm
// it must be the same every time (the determinism tests run it twice and
// compare, including the final tick count).
public class TestInterleave {
	static int shared;
	static StringBuffer trace = new StringBuffer();
	static final Object lock = new Object();
	static int turns;

	public static void main(String[] args) throws Exception {
		Thread[] ts = new Thread[5];
		for (int i = 0; i < ts.length; i++) {
			final int id = i;
			ts[i] = new Thread() {
				public void run() {
					for (int k = 0; k < 3000; k++) {
						int v = shared;          // read-modify-write race
						if ((k + id) % 7 == 0) Thread.yield();
						shared = v + 1;
						if (k % 500 == 0) trace.append((char) ('a' + id));
						if (k % 1000 == id) {
							try {
								Thread.sleep(id + 1);
							} catch (InterruptedException e) {
							}
						}
					}
					synchronized (lock) {
						turns = turns * 31 + id;
						lock.notifyAll();
					}
				}
			};
			ts[i].start();
		}
		synchronized (lock) {
			while (trace.length() < 30) lock.wait(5);
		}
		for (int i = 0; i < ts.length; i++) ts[i].join();
		System.out.println("shared " + shared);
		System.out.println("trace " + trace);
		System.out.println("turns " + turns);
		System.out.println("time " + System.currentTimeMillis());
	}
}
