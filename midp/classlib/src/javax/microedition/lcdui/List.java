package javax.microedition.lcdui;

import jmevm.midp.Events;

/**
 * A screen of choices. In an implicit list the highlighted element is the
 * selected one, and fire (or a touch) invokes the select command.
 */
public class List extends Screen implements Choice {
    public static final Command SELECT_COMMAND = new Command("", Command.SCREEN, 0);

    private final int type;
    private final Choices choices;
    private Command selectCommand = SELECT_COMMAND;

    public List(String title, int listType) {
        this(title, listType, new String[0], null);
    }

    public List(String title, int listType, String[] stringElements, Image[] imageElements) {
        if (listType != IMPLICIT && listType != EXCLUSIVE && listType != MULTIPLE) {
            throw new IllegalArgumentException("list type " + listType);
        }
        if (stringElements == null) {
            throw new NullPointerException();
        }
        this.title = title;
        type = listType;
        choices = new Choices(listType, stringElements, imageElements);
    }

    private void changed() {
        Display.repaintIfShown(this);
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

    public void insert(int elementNum, String stringPart, Image imagePart) {
        choices.insert(elementNum, stringPart, imagePart);
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

    public void setSelectCommand(Command command) {
        if (type != IMPLICIT) {
            return;
        }
        if (command != null && command != SELECT_COMMAND) {
            addCommand(command);
        }
        selectCommand = command;
    }

    public void removeCommand(Command cmd) {
        if (cmd == selectCommand) {
            selectCommand = null;
        }
        super.removeCommand(cmd);
    }

    // ---- on the screen

    int contentHeight(int width) {
        return choices.height(width);
    }

    void paintContent(Graphics g, int width, int height) {
        choices.paint(g, width, true);
    }

    private void reveal() {
        int w = Display.width;
        reveal(choices.top(choices.highlight, w), choices.size() > 0 ? choices.rowHeight(choices.highlight, w) : 0);
    }

    private void act() {
        if (!choices.act()) {
            return;
        }
        changed();
        if (type == IMPLICIT && selectCommand != null) {
            fire(selectCommand);
        }
    }

    void keyEvent(int type, int keyCode) {
        if (type == KEY_UP) {
            return;
        }
        int action = Canvas.actionOf(keyCode);
        if (action == Canvas.UP || action == Canvas.DOWN) {
            if (choices.move(action == Canvas.UP ? -1 : 1)) {
                reveal();
                changed();
            }
        } else if (action == Canvas.FIRE && type == KEY_DOWN) {
            act();
        }
    }

    void contentPointer(int type, int x, int y) {
        int i = choices.at(y, Display.width);
        if (i < 0) {
            return;
        }
        choices.highlight = i;
        if (this.type == IMPLICIT) {
            choices.setSelectedIndex(i, true);
        }
        changed();
        if (type == Events.POINTER_UP) {
            act();
        }
    }
}
