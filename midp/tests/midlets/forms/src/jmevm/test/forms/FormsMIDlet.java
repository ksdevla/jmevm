package jmevm.test.forms;

import java.util.Date;

import javax.microedition.lcdui.Alert;
import javax.microedition.lcdui.AlertType;
import javax.microedition.lcdui.Choice;
import javax.microedition.lcdui.ChoiceGroup;
import javax.microedition.lcdui.Command;
import javax.microedition.lcdui.CommandListener;
import javax.microedition.lcdui.DateField;
import javax.microedition.lcdui.Display;
import javax.microedition.lcdui.Displayable;
import javax.microedition.lcdui.Form;
import javax.microedition.lcdui.Gauge;
import javax.microedition.lcdui.Image;
import javax.microedition.lcdui.ImageItem;
import javax.microedition.lcdui.Item;
import javax.microedition.lcdui.ItemStateListener;
import javax.microedition.lcdui.List;
import javax.microedition.lcdui.Spacer;
import javax.microedition.lcdui.StringItem;
import javax.microedition.lcdui.TextBox;
import javax.microedition.lcdui.TextField;
import javax.microedition.lcdui.Ticker;
import javax.microedition.midlet.MIDlet;

/**
 * The high-level UI: text fields, choices, gauges, alerts, forms and the
 * other items, checked against the MIDP 2.0 specification. Then a form is
 * shown, and what the keys do to it is printed.
 */
public class FormsMIDlet extends MIDlet implements CommandListener, ItemStateListener {
    private final Command ok = new Command("OK", Command.OK, 1);
    private final Command exit = new Command("Exit", Command.EXIT, 2);

    protected void startApp() {
        try {
            textFields();
            choices();
            gauges();
            alerts();
            forms();
            otherItems();
        } catch (Throwable t) {
            Check.yes("no exception: " + t, false);
        }
        Check.summary();
        show();
    }

    protected void pauseApp() {
    }

    protected void destroyApp(boolean unconditional) {
    }

    static void textFields() {
        final TextField t = new TextField("Name", "ab", 5, TextField.ANY);
        Check.yes("textfield contents", "ab".equals(t.getString()) && t.size() == 2 && t.getMaxSize() == 5);
        Check.fails("textfield longer than its maximum", IllegalArgumentException.class, new Check.Action() {
            public void run() {
                t.setString("123456");
            }
        });
        t.insert("xy", 1);
        Check.eq("textfield insert", t.getString(), "axyb");
        t.delete(1, 2);
        Check.eq("textfield delete", t.getString(), "ab");
        char[] chars = new char[5];
        Check.eq("textfield getChars", t.getChars(chars), 2);
        Check.yes("textfield chars", chars[0] == 'a' && chars[1] == 'b');
        Check.eq("textfield setMaxSize truncates", t.setMaxSize(1) * 10 + t.size(), 11);
        Check.eq("textfield after truncation", t.getString(), "a");
        Check.yes("textfield caret", t.getCaretPosition() >= 0 && t.getCaretPosition() <= t.size());

        final TextField n = new TextField("Number", "", 4, TextField.NUMERIC);
        Check.fails("numeric field rejects letters", IllegalArgumentException.class, new Check.Action() {
            public void run() {
                n.setString("12a");
            }
        });
        n.setString("-12");
        Check.eq("numeric field takes a minus sign", n.getString(), "-12");
        Check.fails("numeric initial text", IllegalArgumentException.class, new Check.Action() {
            public void run() {
                new TextField(null, "x", 4, TextField.NUMERIC);
            }
        });
        TextField p = new TextField("Password", "", 8, TextField.ANY | TextField.PASSWORD);
        Check.eq("password flag", p.getConstraints() & TextField.CONSTRAINT_MASK, TextField.ANY);
        Check.yes("password modifier", (p.getConstraints() & TextField.PASSWORD) != 0);
        Check.fails("zero maximum size", IllegalArgumentException.class, new Check.Action() {
            public void run() {
                new TextField(null, "", 0, TextField.ANY);
            }
        });
        TextBox box = new TextBox("Box", "hello", 10, TextField.ANY);
        Check.yes("textbox", "hello".equals(box.getString()) && box.getMaxSize() == 10);
    }

    static int selectedCount(Choice c) {
        boolean[] flags = new boolean[c.size()];
        return c.getSelectedFlags(flags);
    }

    static void choices() {
        final ChoiceGroup one = new ChoiceGroup("One", Choice.EXCLUSIVE);
        Check.eq("choice append", one.append("a", null) * 100 + one.append("b", null) * 10 + one.append("c", null),
                12);
        Check.eq("exclusive choice selects the first", one.getSelectedIndex(), 0);
        one.setSelectedIndex(2, true);
        Check.yes("exclusive choice select", one.getSelectedIndex() == 2 && !one.isSelected(0));
        one.setSelectedIndex(2, false);
        Check.eq("exclusive choice ignores deselection", one.getSelectedIndex(), 2);
        one.delete(2);
        Check.yes("deleting the selected element selects another",
                one.size() == 2 && selectedCount(one) == 1 && one.getSelectedIndex() >= 0);
        one.set(1, "B", null);
        one.insert(0, "z", null);
        Check.yes("choice set and insert", one.size() == 3 && "z".equals(one.getString(0))
                && "B".equals(one.getString(2)));
        Check.fails("choice index outside", IndexOutOfBoundsException.class, new Check.Action() {
            public void run() {
                one.getString(3);
            }
        });

        ChoiceGroup many = new ChoiceGroup("Many", Choice.MULTIPLE, new String[] {"x", "y", "z"}, null);
        Check.eq("multiple choice has no selected index", many.getSelectedIndex(), -1);
        Check.eq("multiple choice starts unselected", selectedCount(many), 0);
        many.setSelectedFlags(new boolean[] {true, false, true});
        Check.yes("multiple choice flags", selectedCount(many) == 2 && many.isSelected(2) && !many.isSelected(1));
        ChoiceGroup popup = new ChoiceGroup("Popup", Choice.POPUP, new String[] {"p", "q"}, null);
        Check.eq("popup choice selects the first", popup.getSelectedIndex(), 0);
        Check.fails("implicit choice group", IllegalArgumentException.class, new Check.Action() {
            public void run() {
                new ChoiceGroup("x", Choice.IMPLICIT);
            }
        });

        List list = new List("List", Choice.IMPLICIT, new String[] {"one", "two"}, null);
        Check.yes("list", list.size() == 2 && list.getSelectedIndex() == 0 && "two".equals(list.getString(1)));
        list.deleteAll();
        Check.eq("list deleteAll", list.size(), 0);
    }

    static void gauges() {
        final Gauge g = new Gauge("Level", true, 10, 3);
        Check.yes("gauge", g.getValue() == 3 && g.getMaxValue() == 10 && g.isInteractive());
        g.setValue(15);
        Check.eq("gauge clamps to the maximum", g.getValue(), 10);
        g.setValue(-1);
        Check.eq("gauge clamps to zero", g.getValue(), 0);
        g.setValue(8);
        g.setMaxValue(5);
        Check.eq("lower maximum lowers the value", g.getValue(), 5);
        Check.fails("interactive gauge needs a maximum", IllegalArgumentException.class, new Check.Action() {
            public void run() {
                new Gauge(null, true, 0, 0);
            }
        });
        Gauge busy = new Gauge(null, false, Gauge.INDEFINITE, Gauge.CONTINUOUS_RUNNING);
        Check.yes("indefinite gauge", busy.getMaxValue() == Gauge.INDEFINITE
                && busy.getValue() == Gauge.CONTINUOUS_RUNNING);
    }

    static void alerts() {
        final Alert a = new Alert("Note", "text", null, AlertType.INFO);
        Check.eq("alert default timeout", a.getTimeout(), a.getDefaultTimeout());
        a.setTimeout(Alert.FOREVER);
        Check.eq("alert forever", a.getTimeout(), Alert.FOREVER);
        a.setTimeout(1500);
        Check.eq("alert timeout", a.getTimeout(), 1500);
        Check.fails("alert timeout of 0", IllegalArgumentException.class, new Check.Action() {
            public void run() {
                a.setTimeout(0);
            }
        });
        a.setType(AlertType.ERROR);
        Check.yes("alert type and text", a.getType() == AlertType.ERROR && "text".equals(a.getString()));
    }

    static void forms() {
        final Form f = new Form("Form");
        Check.eq("form append text", f.append("hello"), 0);
        Check.yes("text becomes a string item", f.get(0) instanceof StringItem
                && "hello".equals(((StringItem) f.get(0)).getText()));
        final StringItem s = new StringItem("Label", "value");
        Check.eq("form append item", f.append(s), 1);
        f.insert(0, new Spacer(5, 3));
        Check.yes("form insert", f.size() == 3 && f.get(2) == s);
        final Form other = new Form("Other");
        Check.fails("an item belongs to one form", IllegalStateException.class, new Check.Action() {
            public void run() {
                other.append(s);
            }
        });
        f.delete(2);
        other.append(s);
        Check.eq("a removed item can move", other.size(), 1);
        f.deleteAll();
        Check.eq("form deleteAll", f.size(), 0);
        Check.fails("form index outside", IndexOutOfBoundsException.class, new Check.Action() {
            public void run() {
                f.get(0);
            }
        });
        f.setTicker(new Ticker("news"));
        Check.eq("ticker", f.getTicker().getString(), "news");
        f.setTitle("Renamed");
        Check.eq("title", f.getTitle(), "Renamed");
    }

    static void otherItems() {
        Command c = new Command("Save", "Save the game", Command.SCREEN, 3);
        Check.yes("command", "Save".equals(c.getLabel()) && "Save the game".equals(c.getLongLabel())
                && c.getCommandType() == Command.SCREEN && c.getPriority() == 3);
        Spacer sp = new Spacer(5, 3);
        Check.eq("spacer", sp.getMinimumWidth() * 10 + sp.getMinimumHeight(), 53);
        Image img = Image.createImage(6, 4);
        ImageItem ii = new ImageItem("Pic", img, Item.LAYOUT_CENTER, "alt");
        Check.yes("image item", ii.getImage() == img && "alt".equals(ii.getAltText())
                && (ii.getLayout() & Item.LAYOUT_CENTER) == Item.LAYOUT_CENTER);
        DateField d = new DateField("When", DateField.DATE_TIME);
        Check.yes("date field starts empty", d.getDate() == null);
        d.setDate(new Date(123456789000L));
        Check.eq("date field", d.getDate().getTime(), 123456789000L);
        StringItem button = new StringItem(null, "Press", Item.BUTTON);
        Check.eq("button appearance", button.getAppearanceMode(), Item.BUTTON);
    }

    // ---- the form on screen

    private void show() {
        Form f = new Form("jmevm forms");
        f.append(new TextField("Name", "player", 12, TextField.ANY));
        f.append(new ChoiceGroup("Level", Choice.EXCLUSIVE, new String[] {"Easy", "Normal", "Hard"}, null));
        f.append(new Gauge("Volume", true, 10, 5));
        f.append(new StringItem("Score", "1234"));
        f.addCommand(ok);
        f.addCommand(exit);
        f.setCommandListener(this);
        f.setItemStateListener(this);
        Display.getDisplay(this).setCurrent(f);
        System.out.println("form shown");
    }

    public void commandAction(Command c, Displayable d) {
        System.out.println("command " + c.getLabel());
    }

    public void itemStateChanged(Item item) {
        String value = "";
        if (item instanceof ChoiceGroup) {
            value = ((ChoiceGroup) item).getString(((ChoiceGroup) item).getSelectedIndex());
        } else if (item instanceof Gauge) {
            value = String.valueOf(((Gauge) item).getValue());
        } else if (item instanceof TextField) {
            value = ((TextField) item).getString();
        }
        System.out.println("changed " + item.getLabel() + ": " + value);
    }
}
