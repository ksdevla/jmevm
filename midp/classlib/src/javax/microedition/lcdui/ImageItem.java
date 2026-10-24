package javax.microedition.lcdui;

/** An image on a form, with text to show instead when there is none. */
public class ImageItem extends Item {
    private Image image;
    private String altText;
    private final int appearance;

    public ImageItem(String label, Image img, int layout, String altText) {
        this(label, img, layout, altText, PLAIN);
    }

    public ImageItem(String label, Image image, int layout, String altText, int appearanceMode) {
        super(label);
        if (appearanceMode != PLAIN && appearanceMode != HYPERLINK && appearanceMode != BUTTON) {
            throw new IllegalArgumentException("appearance " + appearanceMode);
        }
        this.image = image;
        this.altText = altText;
        this.appearance = appearanceMode;
        setLayout(layout);
    }

    public Image getImage() {
        return image;
    }

    public void setImage(Image img) {
        image = img;
        changed();
    }

    public String getAltText() {
        return altText;
    }

    public void setAltText(String text) {
        altText = text;
        changed();
    }

    public int getAppearanceMode() {
        return appearance;
    }

    int contentHeight(int width) {
        if (image != null) {
            return image.getHeight();
        }
        return altText == null ? 0 : Skin.TEXT.getHeight();
    }

    void paintContent(Graphics g, int width, int height, boolean focused) {
        int align = getLayout() & LAYOUT_CENTER;
        int x = align == LAYOUT_CENTER ? width / 2 : align == LAYOUT_RIGHT ? width : 0;
        int h = align == LAYOUT_CENTER ? Graphics.HCENTER : align == LAYOUT_RIGHT ? Graphics.RIGHT : Graphics.LEFT;
        if (image != null) {
            g.drawImage(image, x, 0, Graphics.TOP | h);
        } else if (altText != null) {
            g.setFont(Skin.TEXT);
            g.setColor(Skin.MUTED);
            g.drawString(altText, x, 0, Graphics.TOP | h);
        }
    }
}
