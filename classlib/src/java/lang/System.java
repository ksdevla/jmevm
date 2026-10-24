package java.lang;

import java.io.PrintStream;

import jmevm.ConsoleOutputStream;

/** The console, the clock, array copying and the platform's properties. */
public final class System {
    public static final PrintStream out = new PrintStream(new ConsoleOutputStream(ConsoleOutputStream.OUT));
    public static final PrintStream err = new PrintStream(new ConsoleOutputStream(ConsoleOutputStream.ERR));

    private System() {
    }

    /** Milliseconds since 1970-01-01 00:00 UTC, by the virtual machine's clock. */
    public static native long currentTimeMillis();

    public static native void arraycopy(Object src, int srcPos, Object dst, int dstPos, int length);

    /** The hash code Object.hashCode() would give, overridden or not. */
    public static native int identityHashCode(Object x);

    /** A property of the platform ("microedition.platform" and the like), or null. */
    public static String getProperty(String key) {
        if (key.length() == 0) {
            throw new IllegalArgumentException("key can't be empty");
        }
        return getProperty0(key);
    }

    private static native String getProperty0(String key);

    public static void exit(int status) {
        Runtime.getRuntime().exit(status);
    }

    public static void gc() {
        Runtime.getRuntime().gc();
    }
}
