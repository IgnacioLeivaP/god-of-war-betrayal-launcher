package javax.microedition.lcdui;

import gownx.N;

public class Graphics {
    public static final int HCENTER = 1, VCENTER = 2, LEFT = 4, RIGHT = 8, TOP = 16, BOTTOM = 32, BASELINE = 64;
    public static final int SOLID = 0, DOTTED = 1;

    private final int dst, w, h;
    private int tx, ty;                 // traslacion
    private int cx, cy, cw, ch;         // recorte, en coordenadas absolutas del buffer
    private int color = 0xFF000000;
    private Font font = Font.getDefaultFont();

    private final Image duena;   // mantiene viva la imagen mientras exista este Graphics

    Graphics(int dst, int w, int h) { this(null, dst, w, h); }

    Graphics(Image duena, int dst, int w, int h) {
        this.duena = duena;
        this.dst = dst;
        this.w = w;
        this.h = h;
        reinicia();
    }

    void reinicia() {
        tx = ty = 0;
        cx = cy = 0;
        cw = w;
        ch = h;
        color = 0xFF000000;
    }

    public void translate(int x, int y) { tx += x; ty += y; }
    public int getTranslateX() { return tx; }
    public int getTranslateY() { return ty; }
    public void setColor(int rgb) { color = 0xFF000000 | rgb; }
    public void setColor(int r, int g, int b) { color = 0xFF000000 | (r & 255) << 16 | (g & 255) << 8 | (b & 255); }
    public int getColor() { return color & 0xFFFFFF; }
    public void setFont(Font f) { if (f != null) font = f; }
    public Font getFont() { return font; }
    public void setStrokeStyle(int s) {}

    public int getClipX() { return cx - tx; }
    public int getClipY() { return cy - ty; }
    public int getClipWidth() { return cw; }
    public int getClipHeight() { return ch; }

    public void setClip(int x, int y, int cw2, int ch2) {
        int x1 = x + tx, y1 = y + ty, x2 = x1 + cw2, y2 = y1 + ch2;
        if (x1 < 0) x1 = 0;
        if (y1 < 0) y1 = 0;
        if (x2 > w) x2 = w;
        if (y2 > h) y2 = h;
        cx = x1;
        cy = y1;
        cw = x2 > x1 ? x2 - x1 : 0;
        ch = y2 > y1 ? y2 - y1 : 0;
    }

    public void clipRect(int x, int y, int cw2, int ch2) {
        int x1 = Math.max(x + tx, cx), y1 = Math.max(y + ty, cy);
        int x2 = Math.min(x + tx + cw2, cx + cw), y2 = Math.min(y + ty + ch2, cy + ch);
        cx = x1;
        cy = y1;
        cw = x2 > x1 ? x2 - x1 : 0;
        ch = y2 > y1 ? y2 - y1 : 0;
    }

    public void drawImage(Image img, int x, int y, int anchor) { dibuja(img, x, y, anchor, 0); }

    /** Para DirectGraphics de Nokia: flip bit 1 = espejo horizontal, bit 2 = vertical. */
    public void dibuja(Image img, int x, int y, int anchor, int flip) {
        if (img == null || cw <= 0 || ch <= 0) return;
        if ((anchor & HCENTER) != 0) x -= img.getWidth() / 2; else if ((anchor & RIGHT) != 0) x -= img.getWidth();
        if ((anchor & VCENTER) != 0) y -= img.getHeight() / 2; else if ((anchor & BOTTOM) != 0) y -= img.getHeight();
        N.drawImage(dst, img.id, x + tx, y + ty, cx, cy, cw, ch, flip);
    }

    public void fillRect(int x, int y, int rw, int rh) {
        if (rw > 0 && rh > 0) N.fillRect(dst, x + tx, y + ty, rw, rh, color, cx, cy, cw, ch);
    }

    public void drawRect(int x, int y, int rw, int rh) {
        if (rw < 0 || rh < 0) return;
        fillRect(x, y, rw + 1, 1);
        fillRect(x, y + rh, rw + 1, 1);
        fillRect(x, y, 1, rh + 1);
        fillRect(x + rw, y, 1, rh + 1);
    }

    public void drawLine(int x1, int y1, int x2, int y2) {
        N.drawLine(dst, x1 + tx, y1 + ty, x2 + tx, y2 + ty, color, cx, cy, cw, ch);
    }

    public void drawRGB(int[] rgb, int off, int scan, int x, int y, int rw, int rh, boolean alpha) {
        N.drawRGB(dst, rgb, off, scan, x + tx, y + ty, rw, rh, alpha ? 1 : 0, cx, cy, cw, ch);
    }

    // Texto con la fuente del sistema: el juego usa su propio atlas; pendiente si hiciera falta.
    public void drawChars(char[] c, int off, int len, int x, int y, int anchor) {}
    public void drawString(String s, int x, int y, int anchor) {}
    public void drawChar(char c, int x, int y, int anchor) {}
}
