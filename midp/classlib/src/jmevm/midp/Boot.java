package jmevm.midp;

/**
 * Where a MIDlet run starts: "jmevm.midp.Boot -midlet [ClassName]". Without
 * a class name, the MIDlet is the first one of the suite (MIDlet-1).
 */
public final class Boot {
    private Boot() {
    }

    public static void main(String[] args) {
        String name = "";
        for (int i = 0; i + 1 < args.length; i++) {
            if (args[i].equals("-midlet")) {
                name = args[i + 1];
            }
        }
        Midlets.run(name);
    }

    /** Ends the program with an exit status (native in src/midp_jme/app.c). */
    public static native void exit(int status);
}
