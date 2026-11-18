package javax.microedition.midlet;

public abstract class MIDlet {
    protected MIDlet() {}
    protected abstract void startApp() throws MIDletStateChangeException;
    protected abstract void pauseApp();
    protected abstract void destroyApp(boolean unconditional) throws MIDletStateChangeException;
    public final void arranca() throws MIDletStateChangeException { startApp(); }   // gow_nx
    public final String getAppProperty(String key) { return null; }
    public final void notifyDestroyed() { gownx.Main.salir = true; }
    public final void notifyPaused() {}
    public final void resumeRequest() {}
    public final boolean platformRequest(String url) { return false; }
}
