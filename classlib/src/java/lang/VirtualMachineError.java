package java.lang;

/** The virtual machine is broken or out of resources. */
public class VirtualMachineError extends Error {
    public VirtualMachineError() {
    }

    public VirtualMachineError(String message) {
        super(message);
    }
}
