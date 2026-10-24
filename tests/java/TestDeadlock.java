// Two threads take two locks in opposite order. jmevm notices that no
// thread can ever continue, says so and exits with code 3 instead of hanging
// (a desktop JVM hangs, so the expected output is written by hand).
public class TestDeadlock {
	static final Object a = new Object();
	static final Object b = new Object();

	public static void main(String[] args) throws Exception {
		Thread other = new Thread() {
			public void run() {
				synchronized (b) {
					pause(20);
					synchronized (a) {
						System.out.println("other got both");
					}
				}
			}
		};
		synchronized (a) {
			other.start();
			pause(10);
			System.out.println("main waits for b");
			synchronized (b) {
				System.out.println("main got both");
			}
		}
	}

	static void pause(int ms) {
		try {
			Thread.sleep(ms);
		} catch (InterruptedException e) {
		}
	}
}
