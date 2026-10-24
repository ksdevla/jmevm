package javax.microedition.lcdui;

/** An action the user can invoke: a label, a type and a priority. */
public class Command {
    public static final int SCREEN = 1;
    public static final int BACK = 2;
    public static final int CANCEL = 3;
    public static final int OK = 4;
    public static final int HELP = 5;
    public static final int STOP = 6;
    public static final int EXIT = 7;
    public static final int ITEM = 8;

    private final String label;
    private final String longLabel;
    private final int commandType;
    private final int priority;

    public Command(String label, int commandType, int priority) {
        this(label, null, commandType, priority);
    }

    public Command(String shortLabel, String longLabel, int commandType, int priority) {
        if (shortLabel == null) {
            throw new NullPointerException();
        }
        if (commandType < SCREEN || commandType > ITEM) {
            throw new IllegalArgumentException("command type " + commandType);
        }
        this.label = shortLabel;
        this.longLabel = longLabel;
        this.commandType = commandType;
        this.priority = priority;
    }

    public String getLabel() {
        return label;
    }

    public String getLongLabel() {
        return longLabel;
    }

    public int getCommandType() {
        return commandType;
    }

    public int getPriority() {
        return priority;
    }

    /** Whether the command goes back or away (put on the right soft key). */
    boolean leaves() {
        return commandType == BACK || commandType == CANCEL || commandType == EXIT || commandType == STOP;
    }
}
