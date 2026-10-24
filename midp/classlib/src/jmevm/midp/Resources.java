package jmevm.midp;

/** Files of the application's jar (native in src/midp_jme/app.c). */
public final class Resources {
    private Resources() {
    }

    /** A file by its path in the jar (no leading '/'), or null. */
    public static native byte[] read(String path);
}
