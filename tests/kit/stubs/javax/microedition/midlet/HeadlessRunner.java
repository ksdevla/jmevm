package javax.microedition.midlet;

import javax.microedition.lcdui.Canvas;

// Starts a MIDlet without a screen. Canvas.serviceRepaints() paints into an
// int[] framebuffer and prints a hash per frame; the run ends after N frames.
public class HeadlessRunner {
	public static void main(String[] args) throws Exception {
		Canvas.framesToRun = Integer.parseInt(args[1]);
		MIDlet m = (MIDlet) Class.forName(args[0]).newInstance();
		m.startApp();
	}
}
