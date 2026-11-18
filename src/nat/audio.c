// gow_nx: audio de la clase Java gownx.N — mezclador propio sobre el audio de SDL2.
// Cada Player del juego es una "voz": WAV (PCM en memoria) o MP3 (se decodifica al vuelo
// con minimp3, trama a trama, dentro de la retrollamada de audio). La mezcla remuestrea
// cada voz a la frecuencia del dispositivo, asi que no hay conversion al cargar.
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#include <SDL2/SDL.h>

#include "jvm.h"
#include "jvm_util.h"

#define MINIMP3_IMPLEMENTATION
#define MINIMP3_NO_SIMD
#include "minimp3.h"

#define FREC_SALIDA 44100
#define MAX_VOCES 128

typedef struct {
    int usada;          // 0 libre, 1 WAV, 2 MP3
    int sonando;
    int vueltas;        // -1 = infinito; si no, repeticiones que quedan (1 = una pasada)
    int bytes;          // tamano del recurso original (lo identifica en el registro)
    int canales, frec;
    uint32_t frac;      // posicion fraccionaria (16.16) entre muestras de origen
    // WAV
    int16_t *pcm;       // intercalado
    int nframes, pos;
    // MP3
    uint8_t *mp3;
    int mp3_len, mp3_off;
    mp3dec_t dec;
    int16_t buf[MINIMP3_MAX_SAMPLES_PER_FRAME];
    int buf_frames, buf_pos;
} Voz;

static Voz voces[MAX_VOCES];
static SDL_AudioDeviceID disp;
static int audio_ok;
static int registra;     // GOWNX_SONIDOS=1: anota en el registro cada sonido que arranca
static FILE *volcado;   // GOWNX_AUDIO=fichero: vuelca la mezcla (s16 estereo 44100 crudo) para comprobarla sin oirla

// --- MP3: asegura que haya una trama decodificada con muestras por leer. 0 = fin de pista.
static int mp3_llena(Voz *v) {
    while (v->buf_pos >= v->buf_frames) {
        if (v->mp3_off >= v->mp3_len) return 0;
        mp3dec_frame_info_t info;
        int n = mp3dec_decode_frame(&v->dec, v->mp3 + v->mp3_off, v->mp3_len - v->mp3_off, v->buf, &info);
        if (info.frame_bytes <= 0) {
            v->mp3_off = v->mp3_len;
            return 0;
        }
        v->mp3_off += info.frame_bytes;
        if (n > 0) {
            v->buf_pos -= v->buf_frames;   // lo que el remuestreo se paso de la trama anterior
            v->buf_frames = n;
            v->canales = info.channels;
            v->frec = info.hz;
        }
    }
    return 1;
}

static void voz_rebobina(Voz *v) {
    v->frac = 0;
    v->pos = 0;
    if (v->usada == 2) {
        v->mp3_off = 0;
        v->buf_frames = v->buf_pos = 0;
        mp3dec_init(&v->dec);
    }
}

// Muestra actual (estereo) de la voz. 0 = la voz se acabo (y no le quedan vueltas).
static int voz_muestra(Voz *v, int *l, int *r) {
    for (int intento = 0; intento < 2; intento++) {
        const int16_t *p = NULL;
        if (v->usada == 1) {
            if (v->pos < v->nframes) p = v->pcm + v->pos * v->canales;
        } else if (mp3_llena(v)) {
            p = v->buf + v->buf_pos * v->canales;
        }
        if (p) {
            *l = p[0];
            *r = v->canales > 1 ? p[1] : p[0];
            return 1;
        }
        // fin de pista: otra vuelta o se para
        if (v->vueltas > 0) v->vueltas--;
        if (v->vueltas == 0) return 0;
        voz_rebobina(v);
    }
    return 0;
}

static void voz_avanza(Voz *v) {
    v->frac += (uint32_t) (((uint64_t) v->frec << 16) / FREC_SALIDA);
    int n = v->frac >> 16;
    v->frac &= 0xFFFF;
    if (v->usada == 1) v->pos += n; else v->buf_pos += n;   // (mp3_llena absorbe el exceso de trama)
}

static void mezcla_cb(void *ud, Uint8 *stream, int len) {
    (void) ud;
    int16_t *out = (int16_t *) stream;
    int frames = len / 4;
    static int32_t acc[8192 * 2];
    if (frames > 8192) frames = 8192;
    memset(acc, 0, sizeof(int32_t) * frames * 2);
    for (int i = 0; i < MAX_VOCES; i++) {
        Voz *v = &voces[i];
        if (!v->usada || !v->sonando) continue;
        for (int f = 0; f < frames; f++) {
            int l, r;
            if (!voz_muestra(v, &l, &r)) {
                v->sonando = 0;
                break;
            }
            acc[f * 2] += l;
            acc[f * 2 + 1] += r;
            voz_avanza(v);
        }
    }
    for (int k = 0; k < frames * 2; k++) {
        int32_t s = acc[k];
        out[k] = (int16_t) (s > 32767 ? 32767 : s < -32768 ? -32768 : s);
    }
    if (len / 4 > frames) memset(out + frames * 2, 0, (size_t) (len / 4 - frames) * 4);
    if (volcado) fwrite(out, 4, (size_t) frames, volcado);
}

static void audio_abre(void) {
    if (disp || audio_ok < 0) return;
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        jvm_printf("gow_nx: sin audio: %s\n", SDL_GetError());
        audio_ok = -1;
        return;
    }
    SDL_AudioSpec quiero, tengo;
    SDL_zero(quiero);
    quiero.freq = FREC_SALIDA;
    quiero.format = AUDIO_S16SYS;
    quiero.channels = 2;
    quiero.samples = 1024;
    quiero.callback = mezcla_cb;
    disp = SDL_OpenAudioDevice(NULL, 0, &quiero, &tengo, 0);
    if (!disp) {
        jvm_printf("gow_nx: sin audio: %s\n", SDL_GetError());
        audio_ok = -1;
        return;
    }
    audio_ok = 1;
    registra = getenv("GOWNX_SONIDOS") != NULL;
    if (getenv("GOWNX_AUDIO")) volcado = fopen(getenv("GOWNX_AUDIO"), "wb");
    SDL_PauseAudioDevice(disp, 0);
    jvm_printf("gow_nx: audio %d Hz listo\n", tengo.freq);
}

// --- WAV: localiza los trozos "fmt " y "data" y deja PCM de 16 bits en la voz.
static int wav_carga(Voz *v, const uint8_t *d, int len) {
    if (len < 44 || memcmp(d, "RIFF", 4) || memcmp(d + 8, "WAVE", 4)) return 0;
    int canales = 0, frec = 0, bits = 0, formato = 0;
    const uint8_t *datos = NULL;
    int ndatos = 0;
    for (int p = 12; p + 8 <= len;) {
        int tam = d[p + 4] | d[p + 5] << 8 | d[p + 6] << 16 | d[p + 7] << 24;
        if (tam < 0 || p + 8 + tam > len) tam = len - p - 8;
        if (!memcmp(d + p, "fmt ", 4) && tam >= 16) {
            formato = d[p + 8] | d[p + 9] << 8;
            canales = d[p + 10] | d[p + 11] << 8;
            frec = d[p + 12] | d[p + 13] << 8 | d[p + 14] << 16 | d[p + 15] << 24;
            bits = d[p + 22] | d[p + 23] << 8;
        } else if (!memcmp(d + p, "data", 4)) {
            datos = d + p + 8;
            ndatos = tam;
        }
        p += 8 + tam + (tam & 1);
    }
    if (formato != 1 || !datos || canales < 1 || canales > 2 || frec <= 0 || (bits != 8 && bits != 16)) return 0;
    int nmuestras = ndatos / (bits / 8);
    v->pcm = (int16_t *) malloc((size_t) (nmuestras > 0 ? nmuestras : 1) * 2);
    if (!v->pcm) return 0;
    if (bits == 16) {
        memcpy(v->pcm, datos, (size_t) nmuestras * 2);
    } else {
        for (int i = 0; i < nmuestras; i++) v->pcm[i] = (int16_t) ((datos[i] - 128) << 8);
    }
    v->canales = canales;
    v->frec = frec;
    v->nframes = nmuestras / canales;
    return 1;
}

void registra_sonido(int bytes);

#define ARG_I(n) localvar_getInt(runtime->localvar, (n))
#define ARG_R(n) ((Instance *) localvar_getRefer(runtime->localvar, (n)))

// int sndLoad(byte[] datos, int tipo)   tipo: 1 = WAV, 2 = MP3.  -1 = no se pudo.
static s32 n_sndLoad(Runtime *runtime, JClass *clazz) {
    Instance *arr = ARG_R(0);
    int tipo = ARG_I(1);
    int id = -1;
    audio_abre();
    if (arr && audio_ok > 0) {
        const uint8_t *d = (const uint8_t *) jarray_body(arr);
        int len = jarray_length(arr);
        int ok = 0;
        static Voz tmp;   // la Voz es grande (buffer de trama): no va en la pila
        memset(&tmp, 0, sizeof(tmp));
        if (tipo == 2) {
            tmp.mp3 = (uint8_t *) malloc((size_t) len);
            if (tmp.mp3) {
                memcpy(tmp.mp3, d, (size_t) len);
                tmp.mp3_len = len;
                tmp.usada = 2;
                tmp.canales = 2;
                tmp.frec = 44100;
                mp3dec_init(&tmp.dec);
                ok = 1;
            }
        } else if (wav_carga(&tmp, d, len)) {
            tmp.usada = 1;
            ok = 1;
        }
        if (ok) {
            tmp.bytes = len;
            SDL_LockAudioDevice(disp);
            for (int i = 0; i < MAX_VOCES; i++) {
                if (!voces[i].usada) {
                    voces[i] = tmp;
                    id = i;
                    break;
                }
            }
            SDL_UnlockAudioDevice(disp);
            if (id < 0) {
                free(tmp.pcm);
                free(tmp.mp3);
            }
        }
    }
    push_int(runtime->stack, id);
    return 0;
}

static Voz *voz(int id) { return (id >= 0 && id < MAX_VOCES && voces[id].usada) ? &voces[id] : NULL; }

// void sndStart(int id, int vueltas)   (re)arranca desde donde este; vueltas -1 = infinito
static s32 n_sndStart(Runtime *runtime, JClass *clazz) {
    int id = ARG_I(0), vueltas = ARG_I(1);
    if (!disp) return 0;
    SDL_LockAudioDevice(disp);
    Voz *v = voz(id);
    if (v) {
        if (!v->sonando) {
            // una voz que termino su pasada se rebobina sola al volver a arrancar
            if (v->usada == 1 ? v->pos >= v->nframes : v->mp3_off >= v->mp3_len && v->buf_pos >= v->buf_frames) voz_rebobina(v);
            v->vueltas = vueltas;
            v->sonando = 1;
        }
    }
    int bytes = v ? v->bytes : 0;
    SDL_UnlockAudioDevice(disp);
    if (registra && bytes) registra_sonido(bytes);
    return 0;
}

static s32 n_sndStop(Runtime *runtime, JClass *clazz) {
    int id = ARG_I(0);
    if (!disp) return 0;
    SDL_LockAudioDevice(disp);
    Voz *v = voz(id);
    if (v) v->sonando = 0;
    SDL_UnlockAudioDevice(disp);
    return 0;
}

static s32 n_sndRewind(Runtime *runtime, JClass *clazz) {
    int id = ARG_I(0);
    if (!disp) return 0;
    SDL_LockAudioDevice(disp);
    Voz *v = voz(id);
    if (v) voz_rebobina(v);
    SDL_UnlockAudioDevice(disp);
    return 0;
}

static s32 n_sndPlaying(Runtime *runtime, JClass *clazz) {
    Voz *v = voz(ARG_I(0));
    push_int(runtime->stack, v && v->sonando ? 1 : 0);
    return 0;
}

static s32 n_sndFree(Runtime *runtime, JClass *clazz) {
    int id = ARG_I(0);
    if (!disp) return 0;
    SDL_LockAudioDevice(disp);
    Voz *v = voz(id);
    if (v) {
        free(v->pcm);
        free(v->mp3);
        memset(v, 0, sizeof(*v));
    }
    SDL_UnlockAudioDevice(disp);
    return 0;
}

// Una linea por sonido que arranca: milisegundos desde el arranque y tamano del recurso
// (tools/sonidos_log.py los traduce a nombres con la tabla de Build-SfxPack.py).
void registra_sonido(int bytes) {
    jvm_printf("snd %u %d\n", (unsigned) SDL_GetTicks(), bytes);
}

void gownx_audio_cierra(void) {
    if (disp) {
        SDL_CloseAudioDevice(disp);   // espera a que termine la retrollamada
        disp = 0;
    }
    if (volcado) {
        fclose(volcado);
        volcado = NULL;
    }
}

static java_native_method TABLA_AUDIO[] = {
    {"gownx/N", "sndLoad", "([BI)I", n_sndLoad},
    {"gownx/N", "sndStart", "(II)V", n_sndStart},
    {"gownx/N", "sndStop", "(I)V", n_sndStop},
    {"gownx/N", "sndRewind", "(I)V", n_sndRewind},
    {"gownx/N", "sndPlaying", "(I)I", n_sndPlaying},
    {"gownx/N", "sndFree", "(I)V", n_sndFree},
};

void gownx_registra_audio(MiniJVM *jvm) {
    native_reg_lib(jvm, TABLA_AUDIO, sizeof(TABLA_AUDIO) / sizeof(TABLA_AUDIO[0]));
}
