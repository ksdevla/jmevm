package java.lang.ref;

/** A reference that does not keep its object alive. */
public class WeakReference extends Reference {
    public WeakReference(Object referent) {
        super(referent);
    }
}
