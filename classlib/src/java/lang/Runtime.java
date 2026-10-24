package java.lang;

/** The running virtual machine: its heap and its exit. */
public class Runtime {
    private static final Runtime RUNTIME = new Runtime();

    private Runtime() {
    }

    public static Runtime getRuntime() {
        return RUNTIME;
    }

    /** Stops the virtual machine with a status code; does not return. */
    public native void exit(int status);

    public native long freeMemory();

    public native long totalMemory();

    public native void gc();
}
