package javax.microedition.media;

import gownx.N;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;

public final class Manager {
    /** Cada Player es una voz del mezclador nativo (src/nat/audio.c): WAV o MP3. */
    public static Player createPlayer(InputStream in, String type) throws IOException, MediaException {
        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        byte[] buf = new byte[16384];
        int n;
        while ((n = in.read(buf, 0, buf.length)) > 0) bos.write(buf, 0, n);
        byte[] datos = bos.toByteArray();
        int tipo = "audio/mpeg".equals(type) ? 2 : 1;
        if (!"audio/mpeg".equals(type) && !"audio/x-wav".equals(type)) throw new MediaException("tipo no soportado: " + type);
        final int id = N.sndLoad(datos, tipo);
        if (id < 0) throw new MediaException("no se pudo cargar el sonido");
        return new Player() {
            private int estado = REALIZED;
            private int vueltas = 1;

            public void close() {
                if (estado != CLOSED) {
                    estado = CLOSED;
                    N.sndFree(id);
                }
            }

            public int getState() {
                if (estado == STARTED && N.sndPlaying(id) == 0) estado = PREFETCHED;
                return estado;
            }

            public void setLoopCount(int c) {
                if (c == 0) throw new IllegalArgumentException();
                if (getState() == STARTED || estado == CLOSED) throw new IllegalStateException();
                vueltas = c;
            }

            public long setMediaTime(long t) {
                if (estado == CLOSED) throw new IllegalStateException();
                N.sndRewind(id);
                return 0;
            }

            public void start() {
                if (estado == CLOSED) throw new IllegalStateException();
                N.sndStart(id, vueltas);
                estado = STARTED;
            }

            public void stop() {
                if (estado == CLOSED) throw new IllegalStateException();
                if (estado == STARTED) {
                    N.sndStop(id);
                    estado = PREFETCHED;
                }
            }
        };
    }
}
