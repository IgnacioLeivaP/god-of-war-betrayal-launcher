package javax.microedition.lcdui;

public abstract class Displayable {
    public boolean isShown() { return Display.actual == this; }
    public int getWidth() { return Display.ancho; }
    public int getHeight() { return Display.alto; }
}
