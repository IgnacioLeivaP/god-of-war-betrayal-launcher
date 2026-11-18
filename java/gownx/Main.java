package gownx;

import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Display;
import javax.microedition.midlet.MIDlet;

/** Arranque: crea el MIDlet del juego y atiende ventana y teclas en el hilo principal. */
public final class Main {
    public static volatile boolean salir;

    public static void main(String[] args) throws Exception {
        String clase = args.length > 0 ? args[0] : "GOWMIDlet";
        int w = args.length > 1 ? Integer.parseInt(args[1]) : 240;
        int h = args.length > 2 ? Integer.parseInt(args[2]) : 320;
        int escala = args.length > 3 ? Integer.parseInt(args[3]) : 2;
        Display.ancho = w;
        Display.alto = h;
        N.init(w, h, escala);
        MIDlet m = (MIDlet) Class.forName(clase).newInstance();
        m.arranca();
        while (!salir) {
            int ev;
            while ((ev = N.poll()) != 0) {
                if (ev == -1) {
                    salir = true;
                    break;
                }
                Canvas c = Display.actual;
                if (c != null) {
                    int tecla = (short) ev;
                    if ((ev >> 16) == 1) c.keyPressed(tecla); else c.keyReleased(tecla);
                }
            }
            Thread.sleep(2);
        }
        System.out.println("gow_nx: fin");
        System.exit(0);
    }
}
