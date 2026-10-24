package java.lang;

/** A thread's stack overflowed, usually through deep recursion. */
public class StackOverflowError extends VirtualMachineError {
    public StackOverflowError() {
    }

    public StackOverflowError(String message) {
        super(message);
    }
}
