// A thread that needs a class another thread is still initializing waits
// for the initializer to finish and then sees its result; a failed
// initializer is seen as failed by every thread. Output must match a
// desktop JVM.
public class TestThreadInit {
	static class Slow {
		static int value;

		static {
			log("init start");
			try {
				Thread.sleep(50);
			} catch (InterruptedException e) {
			}
			value = 42;
			log("init end");
		}
	}

	static class Broken {
		static int value = 1 / zero();
	}

	static int zero() {
		try {
			Thread.sleep(30);
		} catch (InterruptedException e) {
		}
		return 0;
	}

	static synchronized void log(String s) {
		System.out.println(s);
	}

	public static void main(String[] args) throws Exception {
		Thread other = new Thread() {
			public void run() {
				try {
					Thread.sleep(10);
				} catch (InterruptedException e) {
				}
				log("other sees " + Slow.value);
			}
		};
		other.start();
		int v = Slow.value;
		other.join();
		log("main read " + v);

		Thread second = new Thread() {
			public void run() {
				try {
					Thread.sleep(10);
				} catch (InterruptedException e) {
				}
				try {
					log("second sees " + Broken.value);
				} catch (Throwable e) {
					log("second: " + e.getClass().getName());
				}
			}
		};
		second.start();
		try {
			log("main sees " + Broken.value);
		} catch (Throwable e) {
			log("main: " + e.getClass().getName());
		}
		second.join();
		log("done");
	}
}
