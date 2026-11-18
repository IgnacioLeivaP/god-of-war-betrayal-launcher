package gownx;

/** Nativos de gow_nx (src/nat/nat.c): video, teclas y audio sobre SDL2. */
public final class N {
    private N() {}

    // --- video. Una imagen es un indice a un buffer ARGB; la 0 es la pantalla.
    public static native void init(int w, int h, int scale);
    public static native int imgDecode(byte[] data, int off, int len);
    public static native int imgNew(int w, int h, int argb);
    public static native int imgRegion(int src, int x, int y, int w, int h, int transform);
    public static native void imgFree(int img);
    public static native int imgW(int img);
    public static native int imgH(int img);
    /** flip: bit 1 = espejo horizontal, bit 2 = espejo vertical. */
    public static native void drawImage(int dst, int src, int x, int y, int cx, int cy, int cw, int ch, int flip);
    public static native void fillRect(int dst, int x, int y, int w, int h, int argb, int cx, int cy, int cw, int ch);
    public static native void drawLine(int dst, int x1, int y1, int x2, int y2, int argb, int cx, int cy, int cw, int ch);
    public static native void drawRGB(int dst, int[] rgb, int off, int scan, int x, int y, int w, int h, int alpha, int cx, int cy, int cw, int ch);
    /** Publica la pantalla (imagen 0) para que el hilo principal la muestre. */
    public static native void present();
    /** Cronometro del pintado: 0 = empieza paint, 1 = termino (tras present). */
    public static native void mide(int fase);

    // --- audio (src/nat/audio.c). tipo: 1 = WAV, 2 = MP3. vueltas: -1 = infinito.
    public static native int sndLoad(byte[] data, int tipo);
    public static native void sndStart(int id, int vueltas);
    public static native void sndStop(int id);
    public static native void sndRewind(int id);
    public static native int sndPlaying(int id);
    public static native void sndFree(int id);

    // --- eventos (solo desde el hilo principal). 0 = nada; -1 = salir;
    //     si no: (1 = pulsada, 2 = soltada) << 16 | (codigo de tecla MIDP & 0xFFFF)
    public static native int poll();
}
