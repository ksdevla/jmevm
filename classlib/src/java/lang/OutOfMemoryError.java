package java.lang;

/** The heap has no room for an object, even after collecting garbage. */
public class OutOfMemoryError extends VirtualMachineError {
    public OutOfMemoryError() {
    }

    public OutOfMemoryError(String message) {
        super(message);
    }
}
