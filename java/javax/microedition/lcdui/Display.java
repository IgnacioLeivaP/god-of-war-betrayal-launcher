package javax.microedition.lcdui;

import javax.microedition.midlet.MIDlet;

public class Display {
    public static int ancho = 569, alto = 320;
    public static volatile Canvas actual;
    private static final Display unico = new Display();

    public static Display getDisplay(MIDlet m) { return unico; }

    public void setCurrent(Displayable d) {
        if (d instanceof Canvas) {
            actual = (Canvas) d;
            actual.showNotify();
        }
    }

    public Displayable getCurrent() { return actual; }
}
