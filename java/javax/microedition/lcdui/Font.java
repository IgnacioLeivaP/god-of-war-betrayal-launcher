package javax.microedition.lcdui;

/** Fuente del sistema: el juego trae la suya en un atlas, esta solo da medidas. */
public final class Font {
    private static final Font unica = new Font();

    public static Font getFont(int face, int style, int size) { return unica; }
    public static Font getDefaultFont() { return unica; }
    public int getHeight() { return 12; }
    public int getBaselinePosition() { return 10; }
    public int charsWidth(char[] c, int off, int len) { return len * 6; }
    public int charWidth(char c) { return 6; }
    public int stringWidth(String s) { return s.length() * 6; }
}
