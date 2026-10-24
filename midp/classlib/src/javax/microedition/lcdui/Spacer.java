package javax.microedition.lcdui;

/** Empty space on a form. It has no label and no commands. */
public class Spacer extends Item {
    private int minWidth, minHeight;

    public Spacer(int minWidth, int minHeight) {
        super(null);
        setMinimumSize(minWidth, minHeight);
    }

    public void setMinimumSize(int minWidth, int minHeight) {
        if (minWidth < 0 || minHeight < 0) {
            throw new IllegalArgumentException("size " + minWidth + "x" + minHeight);
        }
        this.minWidth = minWidth;
        this.minHeight = minHeight;
        changed();
    }

    public int getMinimumWidth() {
        return minWidth;
    }

    public int getMinimumHeight() {
        return minHeight;
    }

    public void setLabel(String label) {
        throw new IllegalStateException("a spacer has no label");
    }

    public void addCommand(Command cmd) {
        throw new IllegalStateException("a spacer has no commands");
    }

    public void setDefaultCommand(Command cmd) {
        throw new IllegalStateException("a spacer has no commands");
    }

    int contentHeight(int width) {
        return minHeight;
    }

    void paintContent(Graphics g, int width, int height, boolean focused) {
    }
}
