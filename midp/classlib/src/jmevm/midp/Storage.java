package jmevm.midp;

/**
 * Named blobs that last between runs, for record stores (natives in
 * src/midp_jme/storage.c). They are part of save states too.
 */
public final class Storage {
    private Storage() {
    }

    public static native byte[] read(String key);

    public static native boolean write(String key, byte[] data);

    /** False if there was no such blob. */
    public static native boolean delete(String key);

    public static native String[] list();
}
