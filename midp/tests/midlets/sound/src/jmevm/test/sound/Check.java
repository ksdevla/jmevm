package jmevm.test.sound;

/** Prints "ok what" for each check that holds and "FAIL what: ..." for each that does not. */
final class Check {
    static int checks, failures;

    interface Action {
        void run() throws Exception;
    }

    private Check() {
    }

    static void yes(String what, boolean holds) {
        checks++;
        if (holds) {
            System.out.println("ok " + what);
        } else {
            failures++;
            System.out.println("FAIL " + what);
        }
    }

    static void eq(String what, long got, long want) {
        checks++;
        if (got == want) {
            System.out.println("ok " + what);
        } else {
            failures++;
            System.out.println("FAIL " + what + ": got " + got + " (0x" + Long.toString(got, 16)
                    + "), want " + want + " (0x" + Long.toString(want, 16) + ")");
        }
    }

    static void eq(String what, String got, String want) {
        checks++;
        if (got == null ? want == null : got.equals(want)) {
            System.out.println("ok " + what);
        } else {
            failures++;
            System.out.println("FAIL " + what + ": got " + got + ", want " + want);
        }
    }

    /** Checks that the action throws an exception of the given class (or a subclass). */
    static void fails(String what, Class want, Action action) {
        checks++;
        String got = "nothing";
        try {
            action.run();
        } catch (Throwable t) {
            if (want.isInstance(t)) {
                System.out.println("ok " + what);
                return;
            }
            got = t.toString();
        }
        failures++;
        System.out.println("FAIL " + what + ": threw " + got + ", want " + want.getName());
    }

    static void summary() {
        System.out.println(checks + " checks, " + failures + " failures");
    }
}
