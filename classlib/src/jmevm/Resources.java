package jmevm;

/** The application's resources, as the host provides them (a JAR's entries). */
public final class Resources {
    private Resources() {
    }

    /** The bytes of a resource by its path within the application, or null. */
    public static native byte[] read(String name);
}
