// Malformed bytecode (made by MakeBadClasses) is rejected with VerifyError
// before it runs, and the error can be caught. jmevm reports it when the
// method is first called rather than when the class loads, so the expected
// output is written by hand.
public class TestVerify {
	public static void main(String[] args) {
		for (int i = 1; i <= 6; i++) {
			try {
				Class.forName("Bad" + i).newInstance();
				System.out.println("bad" + i + " ran");
			} catch (Throwable e) {
				System.out.println("bad" + i + " " + e.getClass().getName());
			}
		}
		System.out.println("still running");
	}
}
