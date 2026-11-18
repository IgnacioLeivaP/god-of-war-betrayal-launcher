package javax.microedition.lcdui;

public abstract class Canvas extends Displayable {
    public static final int UP = 1, DOWN = 6, LEFT = 2, RIGHT = 5, FIRE = 8;
    public static final int GAME_A = 9, GAME_B = 10, GAME_C = 11, GAME_D = 12;
    public static final int KEY_NUM0 = 48, KEY_NUM1 = 49, KEY_NUM2 = 50, KEY_NUM3 = 51, KEY_NUM4 = 52;
    public static final int KEY_NUM5 = 53, KEY_NUM6 = 54, KEY_NUM7 = 55, KEY_NUM8 = 56, KEY_NUM9 = 57;
    public static final int KEY_STAR = 42, KEY_POUND = 35;

    private volatile boolean pendiente;
    private Graphics g;

    protected abstract void paint(Graphics g);
    public void keyPressed(int k) {}
    public void keyReleased(int k) {}
    public void keyRepeated(int k) {}
    public void showNotify() {}
    public void hideNotify() {}
    public void setFullScreenMode(boolean b) {}
    public final void repaint() { pendiente = true; }
    public final void repaint(int x, int y, int w, int h) { pendiente = true; }

    public final void serviceRepaints() {
        if (pendiente) {
            pendiente = false;
            if (g == null) g = new Graphics(0, Display.ancho, Display.alto);
            g.reinicia();
            gownx.N.mide(0);
            paint(g);
            gownx.N.present();
            gownx.N.mide(1);
        }
    }

    public int getGameAction(int k) {
        switch (k) {
            case -1: return UP;
            case -2: return DOWN;
            case -3: return LEFT;
            case -4: return RIGHT;
            case -5: return FIRE;
        }
        return 0;
    }
}
