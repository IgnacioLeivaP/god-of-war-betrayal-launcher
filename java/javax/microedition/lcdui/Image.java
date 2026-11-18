package javax.microedition.lcdui;

import gownx.N;

public class Image {
    public final int id;      // indice del buffer nativo
    private final int w, h;
    private final boolean mutable;

    private Image(int id, boolean mutable) {
        if (id < 0) throw new IllegalArgumentException("imagen no valida");
        this.id = id;
        this.w = N.imgW(id);
        this.h = N.imgH(id);
        this.mutable = mutable;
    }

    public static Image createImage(int w, int h) { return new Image(N.imgNew(w, h, 0xFFFFFFFF), true); }

    public static Image createImage(byte[] data, int off, int len) { return new Image(N.imgDecode(data, off, len), false); }

    public static Image createImage(Image src, int x, int y, int w, int h, int transform) {
        return new Image(N.imgRegion(src.id, x, y, w, h, transform), false);
    }

    public Graphics getGraphics() { return new Graphics(this, id, w, h); }

    /** Cuando el juego suelta la imagen (cambio de nivel), se libera su buffer nativo. */
    protected void finalize() {
        N.imgFree(id);
    }
    public int getWidth() { return w; }
    public int getHeight() { return h; }
    public boolean isMutable() { return mutable; }
}
