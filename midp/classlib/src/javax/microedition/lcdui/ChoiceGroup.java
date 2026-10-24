package javax.microedition.lcdui;

import jmevm.midp.Events;

/**
 * Choices on a form: exclusive, multiple, or a pop-up that shows the
 * selected element and opens its list with fire. Up and down move inside
 * the group before the form moves on.
 */
public class ChoiceGroup extends Item implements Choice {
    private final int type;
    private final Choices choices;
    private boolean open;   /* a pop-up showing its list */

    public ChoiceGroup(String label, int choiceType) {
        this(label, choiceType, new String[0], null);
    }

    public ChoiceGroup(String label, int choiceType, String[] stringElements, Image[] imageElements) {
        super(label);
        if (choiceType != EXCLUSIVE && choiceType != MULTIPLE && choiceType != POPUP) {
            throw new IllegalArgumentException("choice type " + choiceType);
        }
        if (stringElements == null) {
            throw new NullPointerException();
        }
        type = choiceType;
        choices = new Choices(choiceType, stringElements, imageElements);
    }

    public int size() {
        return choices.size();
    }

    public String getString(int elementNum) {
        return choices.getString(elementNum);
    }

    public Image getImage(int elementNum) {
        return choices.getImage(elementNum);
    }

    public int append(String stringPart, Image imagePart) {
        int i = choices.append(stringPart, imagePart);
        changed();
        return i;
    }

    public void insert(int elementNum, String stringElement, Image imageElement) {
        choices.insert(elementNum, stringElement, imageElement);
        changed();
    }

    public void delete(int elementNum) {
        choices.delete(elementNum);
        changed();
    }

    public void deleteAll() {
        choices.deleteAll();
        changed();
    }

    public void set(int elementNum, String stringPart, Image imagePart) {
        choices.set(elementNum, stringPart, imagePart);
        changed();
    }

    public boolean isSelected(int elementNum) {
        return choices.isSelected(elementNum);
    }

    public int getSelectedIndex() {
        return choices.getSelectedIndex();
    }

    public int getSelectedFlags(boolean[] selectedArray) {
        return choices.getSelectedFlags(selectedArray);
    }

    public void setSelectedIndex(int elementNum, boolean selected) {
        choices.setSelectedIndex(elementNum, selected);
        changed();
    }

    public void setSelectedFlags(boolean[] selectedArray) {
        choices.setSelectedFlags(selectedArray);
        changed();
    }

    public void setFitPolicy(int fitPolicy) {
        choices.setFitPolicy(fitPolicy);
        changed();
    }

    public int getFitPolicy() {
        return choices.getFitPolicy();
    }

    public void setFont(int elementNum, Font font) {
        choices.setFont(elementNum, font);
        changed();
    }

    public Font getFont(int elementNum) {
        return choices.getFont(elementNum);
    }

    // ---- on a form

    boolean focusable() {
        return true;
    }

    void focusChanged(boolean focused) {
        open = false;
    }

    int contentHeight(int width) {
        if (type == POPUP && !open) {
            return Skin.TEXT.getHeight() + 6;
        }
        return Math.max(choices.height(width), 4);
    }

    void paintContent(Graphics g, int width, int height, boolean focused) {
        if (type == POPUP && !open) {
            g.setColor(Skin.FIELD);
            g.fillRect(0, 0, width, height);
            g.setColor(Skin.BORDER);
            g.drawRect(0, 0, width - 1, height - 1);
            int sel = choices.getSelectedIndex();
            g.setColor(Skin.FOREGROUND);
            g.setFont(Skin.TEXT);
            if (sel >= 0) {
                g.drawString(Skin.fit(choices.getString(sel), Skin.TEXT, width - 20), 4, 3, Graphics.TOP | Graphics.LEFT);
            }
            g.fillTriangle(width - 14, height / 2 - 2, width - 6, height / 2 - 2, width - 10, height / 2 + 3);
            return;
        }
        choices.paint(g, width, focused);
    }

    boolean key(int type, int keyCode) {
        if (type == Displayable.KEY_UP) {
            return super.key(type, keyCode);
        }
        int action = Canvas.actionOf(keyCode);
        boolean list = this.type != POPUP || open;
        if (list && (action == Canvas.UP || action == Canvas.DOWN)) {
            boolean moved = choices.move(action == Canvas.UP ? -1 : 1);
            if (moved) {
                changed();
            }
            return moved;
        }
        if (action == Canvas.FIRE && type == Displayable.KEY_DOWN) {
            if (this.type == POPUP && !open) {
                open = true;
                choices.highlight = Math.max(0, choices.getSelectedIndex());
                changed();
                return true;
            }
            if (choices.act()) {
                open = false;
                userChanged();
                return true;
            }
        }
        return super.key(type, keyCode);
    }

    void pointer(int type, int x, int y, int width, int height) {
        if (type != Events.POINTER_UP) {
            return;
        }
        if (this.type == POPUP && !open) {
            open = true;
            choices.highlight = Math.max(0, choices.getSelectedIndex());
            changed();
            return;
        }
        int i = choices.at(y, width);
        if (i >= 0) {
            choices.highlight = i;
            choices.act();
            open = false;
            userChanged();
        }
    }
}
