package java.lang.ref;

/**
 * A reference object. The collector knows this class: it does not trace
 * referent for weak references and clears it when the object dies.
 */
public abstract class Reference {
    /* The collector finds this field by its name. */
    private Object referent;

    Reference(Object referent) {
        this.referent = referent;
    }

    /** The object referred to, or null once it was collected or cleared. */
    public Object get() {
        return referent;
    }

    public void clear() {
        referent = null;
    }
}
