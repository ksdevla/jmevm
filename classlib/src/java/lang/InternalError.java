package java.lang;

/** An unexpected internal error in the virtual machine. */
public class InternalError extends VirtualMachineError {
    public InternalError() {
    }

    public InternalError(String message) {
        super(message);
    }
}
