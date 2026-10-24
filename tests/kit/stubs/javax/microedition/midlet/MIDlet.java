package javax.microedition.midlet;

public abstract class MIDlet {
	protected MIDlet() {}
	protected abstract void startApp() throws MIDletStateChangeException;
	protected abstract void pauseApp();
	protected abstract void destroyApp(boolean unconditional) throws MIDletStateChangeException;
	public final void notifyDestroyed() { System.exit(0); }
	public final void notifyPaused() {}
	public final void resumeRequest() {}
	public final String getAppProperty(String key) { return null; }
}
