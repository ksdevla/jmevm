package java.lang;

/**
 * The superclass of everything that can be thrown. The virtual machine
 * records the stack when one is constructed and prints it.
 */
public class Throwable {
    /* Read by the virtual machine when it prints the throwable. */
    private String detailMessage;
    /* The frames the virtual machine recorded: (method id, pc) pairs in an int[]. */
    private Object trace;

    public Throwable() {
        fillInStackTrace();
    }

    public Throwable(String message) {
        detailMessage = message;
        fillInStackTrace();
    }

    private native void fillInStackTrace();

    private native void printStackTrace0();

    public String getMessage() {
        return detailMessage;
    }

    public String toString() {
        String message = getMessage();
        String name = getClass().getName();
        return message != null ? name + ": " + message : name;
    }

    /** Prints this throwable and the stack where it was made to System.err. */
    public void printStackTrace() {
        printStackTrace0();
    }
}
