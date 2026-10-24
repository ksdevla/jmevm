package java.lang;

import java.io.ByteArrayInputStream;
import java.io.InputStream;

/**
 * A loaded class, interface or array type. The virtual machine creates the
 * instances and answers the questions about them.
 */
public final class Class {
    /* The virtual machine's id of the class; it fills this in. */
    private int classId;

    private Class() {
    }

    /** Loads and initializes the class of a fully qualified name. */
    public static native Class forName(String className) throws ClassNotFoundException;

    /** A new instance made with the public no-argument constructor. */
    public native Object newInstance() throws InstantiationException, IllegalAccessException;

    public native boolean isInstance(Object obj);

    public native boolean isAssignableFrom(Class cls);

    public native boolean isInterface();

    public native boolean isArray();

    /** The name as the Java language spells it ("java.lang.String", "[I"). */
    public native String getName();

    public String toString() {
        return (isInterface() ? "interface " : "class ") + getName();
    }

    /**
     * A resource of the application, or null. A name that starts with '/' is
     * absolute; any other is relative to this class's package.
     */
    public InputStream getResourceAsStream(String name) {
        if (name.startsWith("/")) {
            name = name.substring(1);
        } else {
            String className = getName();
            int dot = className.lastIndexOf('.');
            if (dot > 0) {
                name = className.substring(0, dot + 1).replace('.', '/') + name;
            }
        }
        byte[] data = jmevm.Resources.read(name);
        return data != null ? new ByteArrayInputStream(data) : null;
    }
}
