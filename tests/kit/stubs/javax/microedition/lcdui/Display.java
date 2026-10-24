package javax.microedition.lcdui;

import javax.microedition.midlet.MIDlet;

public class Display {
	private static Display instance = new Display();
	private Displayable current;
	public static Display getDisplay(MIDlet m) { return instance; }
	public void setCurrent(Displayable d) { current = d; }
	public Displayable getCurrent() { return current; }
}
