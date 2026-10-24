// An exception nobody catches: the program prints what it did so far, the VM
// prints the exception and its stack trace like a desktop JVM, and the exit
// code is 1.
public class TestUncaught {
	public static void main(String[] args) {
		System.out.println("before");
		new TestUncaught().run(3);
	}

	void run(int n) {
		if (n == 0) fail();
		else run(n - 1);
	}

	static void fail() {
		throw new IllegalStateException("boom");
	}
}
