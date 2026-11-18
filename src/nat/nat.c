// gow_nx: nativos de la clase Java gownx.N — video y teclas sobre SDL2.
// Todo el dibujo es por software sobre buffers ARGB propios; SDL solo muestra la pantalla
// final. Asi el hilo del juego nunca toca SDL: pinta en el buffer 0 y lo publica con
// present(); el hilo principal (poll) lo sube a la ventana.
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>

#include "jvm.h"
#include "jvm_util.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "stb_image.h"

// Botones del mando -> teclas del celular: src/nat/controles.c (configurable con controles.txt).
int gownx_boton_midp(int boton);
void gownx_controles_carga(const char *ruta);
void gownx_puentes_cuenta(unsigned long *region, unsigned long *sprite, unsigned long *tiles);   // src/nat/puentes.c
void gownx_puentes_resumen(void);

typedef struct {
    int w, h;
    uint32_t *px;   // ARGB8888
    int opaca;      // todos los pixeles con alfa 255: se puede copiar por filas enteras
} Img;

static Img *imgs;
static int nimgs, cap;
static SDL_mutex *mtx_imgs;
static int vivas;
static size_t bytes_vivos;

static SDL_Window *win;
static SDL_Renderer *ren;
static SDL_Texture *tex, *tex2;
static uint32_t *frente;          // copia publicada de la pantalla
static volatile int hay_frame;
static SDL_mutex *mtx_frame;
static int scr_w, scr_h;

static int img_alloc(int w, int h) {
    if (w <= 0 || h <= 0 || w > 8192 || h > 8192) return -1;
    uint32_t *px = (uint32_t *) malloc((size_t) w * h * 4);
    if (!px) return -1;
    SDL_LockMutex(mtx_imgs);
    int id = -1;
    for (int i = 1; i < nimgs; i++) {      // un hueco de una imagen liberada (la 0 es la pantalla)
        if (!imgs[i].px) {
            id = i;
            break;
        }
    }
    if (id < 0) {
        if (nimgs == cap) {
            cap = cap ? cap * 2 : 256;
            imgs = (Img *) realloc(imgs, sizeof(Img) * cap);
        }
        id = nimgs++;
    }
    imgs[id].w = w;
    imgs[id].h = h;
    imgs[id].px = px;
    imgs[id].opaca = 0;
    vivas++;
    bytes_vivos += (size_t) w * h * 4;
    SDL_UnlockMutex(mtx_imgs);
    return id;
}

// La llama Image.finalize(): el recolector de Java ya no tiene a nadie que use la imagen.
static void img_free(int id) {
    SDL_LockMutex(mtx_imgs);
    if (id > 0 && id < nimgs && imgs[id].px) {
        free(imgs[id].px);
        bytes_vivos -= (size_t) imgs[id].w * imgs[id].h * 4;
        vivas--;
        imgs[id].px = NULL;
        imgs[id].w = imgs[id].h = 0;
    }
    SDL_UnlockMutex(mtx_imgs);
}

// Copia de la entrada de la tabla (la tabla puede moverse si otro hilo crea una imagen).
static int img_get(int id, Img *out) {
    int ok = 0;
    SDL_LockMutex(mtx_imgs);
    if (id >= 0 && id < nimgs && imgs[id].px) {
        *out = imgs[id];
        ok = 1;
    }
    SDL_UnlockMutex(mtx_imgs);
    return ok;
}

// Las dos de un dibujo con un solo bloqueo (se llama cientos de veces por fotograma).
static int img_get2(int a, int b, Img *oa, Img *ob) {
    int ok = 0;
    SDL_LockMutex(mtx_imgs);
    if (a >= 0 && a < nimgs && imgs[a].px && b >= 0 && b < nimgs && imgs[b].px) {
        *oa = imgs[a];
        *ob = imgs[b];
        ok = 1;
    }
    SDL_UnlockMutex(mtx_imgs);
    return ok;
}

static void img_marca_opaca(int id, int opaca) {
    SDL_LockMutex(mtx_imgs);
    if (id >= 0 && id < nimgs && imgs[id].px) imgs[id].opaca = opaca;
    SDL_UnlockMutex(mtx_imgs);
}

// Para el modo de comparacion de los puentes (src/nat/puentes.c): pixeles de una imagen.
int gownx_img_px(int id, uint32_t **px, int *w, int *h) {
    Img im;
    if (!img_get(id, &im)) return 0;
    *px = im.px;
    *w = im.w;
    *h = im.h;
    return 1;
}

static int px_opacos(const uint32_t *px, size_t n) {
    for (size_t i = 0; i < n; i++)
        if (px[i] >> 24 != 255) return 0;
    return 1;
}

#define ARG_I(n) localvar_getInt(runtime->localvar, (n))
#define ARG_R(n) ((Instance *) localvar_getRefer(runtime->localvar, (n)))

static s32 n_init(Runtime *runtime, JClass *clazz) {
    int w = ARG_I(0), h = ARG_I(1), escala = ARG_I(2);
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_JOYSTICK) != 0) {
        jvm_printf("gow_nx: SDL_Init fallo: %s\n", SDL_GetError());
        return 0;
    }
    if (SDL_NumJoysticks() > 0) SDL_JoystickOpen(0);
    gownx_controles_carga("controles.txt");
    mtx_imgs = SDL_CreateMutex();
    mtx_frame = SDL_CreateMutex();
    scr_w = w;
    scr_h = h;
    img_alloc(w, h);   // imagen 0 = pantalla
    memset(imgs[0].px, 0, (size_t) w * h * 4);
    frente = (uint32_t *) calloc((size_t) w * h, 4);
#ifdef __SWITCH__
    win = SDL_CreateWindow("gow_nx", 0, 0, 1280, 720, SDL_WINDOW_FULLSCREEN);
#else
    win = SDL_CreateWindow("God of War: Betrayal (gow_nx)", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                           w * escala, h * escala, SDL_WINDOW_RESIZABLE);
#endif
    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) ren = SDL_CreateRenderer(win, -1, 0);
    // Escalado en dos pasos ("bilineal nitido"): 569x320 no entra un numero entero de veces en
    // 1280x720 (x2,25), y con vecino mas cercano unos pixeles salen de 2 y otros de 3 (se nota
    // al desplazarse). Primero x2 exacto con vecino a una textura intermedia, luego esa a la
    // pantalla con filtro lineal: pixeles uniformes con el borde apenas suavizado.
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, w, h);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    tex2 = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, w * 2, h * 2);
    jvm_printf("gow_nx: video %dx%d listo\n", w, h);
    return 0;
}

static s32 n_imgDecode(Runtime *runtime, JClass *clazz) {
    Instance *arr = ARG_R(0);
    int off = ARG_I(1), len = ARG_I(2);
    int id = -1;
    if (arr && off >= 0 && len > 0 && off + len <= jarray_length(arr)) {
        int w, h, n;
        unsigned char *rgba = stbi_load_from_memory((const unsigned char *) jarray_body(arr) + off, len, &w, &h, &n, 4);
        if (rgba) {
            id = img_alloc(w, h);
            if (id >= 0) {
                Img im;
                img_get(id, &im);
                for (int i = 0; i < w * h; i++) {
                    unsigned char *p = rgba + i * 4;
                    im.px[i] = ((uint32_t) p[3] << 24) | ((uint32_t) p[0] << 16) | ((uint32_t) p[1] << 8) | p[2];
                }
                img_marca_opaca(id, px_opacos(im.px, (size_t) w * h));
            }
            stbi_image_free(rgba);
        } else {
            jvm_printf("gow_nx: PNG no valido (%d bytes): %s\n", len, stbi_failure_reason());
        }
    }
    push_int(runtime->stack, id);
    return 0;
}

static s32 n_imgNew(Runtime *runtime, JClass *clazz) {
    int w = ARG_I(0), h = ARG_I(1);
    uint32_t c = (uint32_t) ARG_I(2);
    int id = img_alloc(w, h);
    if (id >= 0) {
        Img im;
        img_get(id, &im);
        for (int i = 0; i < w * h; i++) im.px[i] = c;
        // Una imagen mutable opaca sigue opaca: todo lo que se pinta encima (drawImage,
        // fillRect, drawLine, drawRGB) deja alfa 255 o no toca el pixel.
        img_marca_opaca(id, c >> 24 == 255);
    }
    push_int(runtime->stack, id);
    return 0;
}

static s32 n_imgRegion(Runtime *runtime, JClass *clazz) {
    int src = ARG_I(0), x = ARG_I(1), y = ARG_I(2), w = ARG_I(3), h = ARG_I(4), t = ARG_I(5);
    Img s;
    int id = -1;
    if (img_get(src, &s) && x >= 0 && y >= 0 && w > 0 && h > 0 && x + w <= s.w && y + h <= s.h) {
        int gira = (t >= 4);                 // 4..7 intercambian ancho y alto
        int dw = gira ? h : w, dh = gira ? w : h;
        id = img_alloc(dw, dh);
        if (id >= 0) {
            Img d;
            img_get(id, &d);
            img_get(src, &s);
            for (int dy = 0; dy < dh; dy++) {
                for (int dx = 0; dx < dw; dx++) {
                    int sx, sy;
                    switch (t) {
                        case 2: sx = w - 1 - dx; sy = dy; break;              // MIRROR
                        case 3: sx = w - 1 - dx; sy = h - 1 - dy; break;      // ROT180
                        case 1: sx = dx; sy = h - 1 - dy; break;              // MIRROR_ROT180
                        case 5: sx = dy; sy = h - 1 - dx; break;              // ROT90
                        case 6: sx = w - 1 - dy; sy = dx; break;              // ROT270
                        case 7: sx = w - 1 - dy; sy = h - 1 - dx; break;      // MIRROR_ROT90
                        case 4: sx = dy; sy = dx; break;                      // MIRROR_ROT270
                        default: sx = dx; sy = dy; break;
                    }
                    d.px[dy * dw + dx] = s.px[(y + sy) * s.w + (x + sx)];
                }
            }
            img_marca_opaca(id, px_opacos(d.px, (size_t) dw * dh));
        }
    }
    push_int(runtime->stack, id);
    return 0;
}

static s32 n_imgFree(Runtime *runtime, JClass *clazz) {
    img_free(ARG_I(0));
    return 0;
}

static s32 n_imgW(Runtime *runtime, JClass *clazz) {
    Img im;
    push_int(runtime->stack, img_get(ARG_I(0), &im) ? im.w : 0);
    return 0;
}

static s32 n_imgH(Runtime *runtime, JClass *clazz) {
    Img im;
    push_int(runtime->stack, img_get(ARG_I(0), &im) ? im.h : 0);
    return 0;
}

static inline uint32_t mezcla(uint32_t d, uint32_t s) {
    uint32_t a = s >> 24;
    if (a == 255) return s;
    if (a == 0) return d;
    uint32_t rb = (((s & 0xFF00FF) * a) + ((d & 0xFF00FF) * (255 - a))) >> 8 & 0xFF00FF;
    uint32_t g = (((s & 0x00FF00) * a) + ((d & 0x00FF00) * (255 - a))) >> 8 & 0x00FF00;
    return 0xFF000000 | rb | g;
}

// Recorta el rectangulo de recorte contra el buffer de destino.
static int recorte(const Img *d, int *cx, int *cy, int *cw, int *ch) {
    int x1 = *cx, y1 = *cy, x2 = *cx + *cw, y2 = *cy + *ch;
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 > d->w) x2 = d->w;
    if (y2 > d->h) y2 = d->h;
    *cx = x1;
    *cy = y1;
    *cw = x2 - x1;
    *ch = y2 - y1;
    return *cw > 0 && *ch > 0;
}

// Dibuja la imagen src en dst con la esquina en (x, y), dentro del recorte (cx, cy, cw, ch),
// todo en coordenadas absolutas del buffer. flip: bit 1 = espejo horizontal, bit 2 = vertical.
// La usan Graphics.drawImage (via N.drawImage) y los puentes de src/nat/puentes.c.
void gownx_blit(int dst, int src, int x, int y, int cx, int cy, int cw, int ch, int flip) {
    Img d, s;
    if (!img_get2(dst, src, &d, &s) || !recorte(&d, &cx, &cy, &cw, &ch)) return;
    int x1 = x < cx ? cx : x, y1 = y < cy ? cy : y;
    int x2 = x + s.w > cx + cw ? cx + cw : x + s.w;
    int y2 = y + s.h > cy + ch ? cy + ch : y + s.h;
    if (x1 >= x2 || y1 >= y2) return;
    if (s.opaca && !(flip & 1)) {
        // Origen opaco (capas de fondo de b, que son imagenes mutables): filas enteras.
        size_t n = (size_t) (x2 - x1) * 4;
        for (int py = y1; py < y2; py++) {
            int sy = py - y;
            if (flip & 2) sy = s.h - 1 - sy;
            memmove(d.px + py * d.w + x1, s.px + sy * s.w + (x1 - x), n);
        }
        return;
    }
    for (int py = y1; py < y2; py++) {
        int sy = py - y;
        if (flip & 2) sy = s.h - 1 - sy;
        const uint32_t *sp = s.px + sy * s.w;
        uint32_t *dp = d.px + py * d.w + x1;
        if (flip & 1) {
            int sx = s.w - 1 - (x1 - x);
            for (int px = x1; px < x2; px++, dp++, sx--) {
                uint32_t c = sp[sx];
                if (c >> 24 == 255) *dp = c; else if (c >> 24) *dp = mezcla(*dp, c);
            }
        } else {
            sp += x1 - x;
            for (int px = x1; px < x2; px++, dp++, sp++) {
                uint32_t c = *sp;
                if (c >> 24 == 255) *dp = c; else if (c >> 24) *dp = mezcla(*dp, c);
            }
        }
    }
}

static s32 n_drawImage(Runtime *runtime, JClass *clazz) {
    gownx_blit(ARG_I(0), ARG_I(1), ARG_I(2), ARG_I(3), ARG_I(4), ARG_I(5), ARG_I(6), ARG_I(7), ARG_I(8));
    return 0;
}

static s32 n_fillRect(Runtime *runtime, JClass *clazz) {
    int dst = ARG_I(0), x = ARG_I(1), y = ARG_I(2), w = ARG_I(3), h = ARG_I(4);
    uint32_t c = (uint32_t) ARG_I(5);
    int cx = ARG_I(6), cy = ARG_I(7), cw = ARG_I(8), ch = ARG_I(9);
    Img d;
    if (!img_get(dst, &d) || !recorte(&d, &cx, &cy, &cw, &ch)) return 0;
    int x1 = x < cx ? cx : x, y1 = y < cy ? cy : y;
    int x2 = x + w > cx + cw ? cx + cw : x + w;
    int y2 = y + h > cy + ch ? cy + ch : y + h;
    for (int py = y1; py < y2; py++) {
        uint32_t *dp = d.px + py * d.w + x1;
        for (int px = x1; px < x2; px++) *dp++ = c;
    }
    return 0;
}

static s32 n_drawLine(Runtime *runtime, JClass *clazz) {
    int dst = ARG_I(0), x1 = ARG_I(1), y1 = ARG_I(2), x2 = ARG_I(3), y2 = ARG_I(4);
    uint32_t c = (uint32_t) ARG_I(5);
    int cx = ARG_I(6), cy = ARG_I(7), cw = ARG_I(8), ch = ARG_I(9);
    Img d;
    if (!img_get(dst, &d) || !recorte(&d, &cx, &cy, &cw, &ch)) return 0;
    int dx = abs(x2 - x1), sx = x1 < x2 ? 1 : -1;
    int dy = -abs(y2 - y1), sy = y1 < y2 ? 1 : -1;
    int err = dx + dy;
    for (int guard = 0; guard < 100000; guard++) {
        if (x1 >= cx && x1 < cx + cw && y1 >= cy && y1 < cy + ch) d.px[y1 * d.w + x1] = c;
        if (x1 == x2 && y1 == y2) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x1 += sx; }
        if (e2 <= dx) { err += dx; y1 += sy; }
    }
    return 0;
}

static s32 n_drawRGB(Runtime *runtime, JClass *clazz) {
    int dst = ARG_I(0);
    Instance *arr = ARG_R(1);
    int off = ARG_I(2), scan = ARG_I(3), x = ARG_I(4), y = ARG_I(5), w = ARG_I(6), h = ARG_I(7), alfa = ARG_I(8);
    int cx = ARG_I(9), cy = ARG_I(10), cw = ARG_I(11), ch = ARG_I(12);
    Img d;
    if (!arr || !img_get(dst, &d) || !recorte(&d, &cx, &cy, &cw, &ch)) return 0;
    const uint32_t *rgb = (const uint32_t *) jarray_body(arr);
    int n = jarray_length(arr);
    for (int j = 0; j < h; j++) {
        int py = y + j;
        if (py < cy || py >= cy + ch) continue;
        for (int i = 0; i < w; i++) {
            int px = x + i;
            if (px < cx || px >= cx + cw) continue;
            int k = off + j * scan + i;
            if (k < 0 || k >= n) continue;
            uint32_t c = rgb[k];
            uint32_t *dp = d.px + py * d.w + px;
            *dp = alfa ? mezcla(*dp, c) : (0xFF000000 | c);
        }
    }
    return 0;
}

// Cronometro del pintado (Canvas.serviceRepaints llama mide(0) antes de paint y mide(1)
// despues de present). Totales acumulados; cada lector (fotos del guion y registro de 5 s)
// guarda su propia instantanea. "ciclo" = tiempo entre el inicio de dos pintados: el bucle
// del juego duerme 20 ms fijos, asi que ciclo - 20 ms es lo que cuesta un fotograma.
static Uint64 med_ini, med_pinta, med_ciclo, med_ult_ini;
static Uint32 med_n, med_nciclo;

static s32 n_mide(Runtime *runtime, JClass *clazz) {
    Uint64 t = SDL_GetPerformanceCounter();
    if (ARG_I(0) == 0) {
        if (med_ult_ini) {
            med_ciclo += t - med_ult_ini;
            med_nciclo++;
        }
        med_ult_ini = med_ini = t;
    } else if (med_ini) {
        med_pinta += t - med_ini;
        med_n++;
    }
    return 0;
}

typedef struct { Uint64 pinta, ciclo; Uint32 n, nciclo; } MedFoto;

// ms medios de pintado y de ciclo desde la instantanea anterior de este lector.
static void med_lee(MedFoto *ant, double *pinta_ms, double *ciclo_ms) {
    double f = 1000.0 / (double) SDL_GetPerformanceFrequency();
    Uint32 n = med_n - ant->n, nc = med_nciclo - ant->nciclo;
    *pinta_ms = n ? (med_pinta - ant->pinta) * f / n : 0.0;
    *ciclo_ms = nc ? (med_ciclo - ant->ciclo) * f / nc : 0.0;
    ant->pinta = med_pinta;
    ant->ciclo = med_ciclo;
    ant->n = med_n;
    ant->nciclo = med_nciclo;
}

static s32 n_present(Runtime *runtime, JClass *clazz) {
    SDL_LockMutex(mtx_frame);
    memcpy(frente, imgs[0].px, (size_t) scr_w * scr_h * 4);
    hay_frame = 1;
    SDL_UnlockMutex(mtx_frame);
    return 0;
}

// Tecla de SDL -> codigo MIDP (0 = no mapeada). En el PC el teclado hace de mando de la
// Switch, con el mismo esquema que controles.c:
//   flechas = cruceta      Z = A/Y (atacar, OK)      X = B (saltar / atras)
//   C = L (bloquear / interactuar)      V = R (cambiar arma)
//   Enter = + (pausa)      Retroceso = - (Power Up)      S = X del mando (blanda izquierda)
// y ademas los numeros, E = * y R = # del celular.
static int tecla_midp(SDL_Keycode k) {
    switch (k) {
        case SDLK_UP: return -1;
        case SDLK_DOWN: return -2;
        case SDLK_LEFT: return -3;
        case SDLK_RIGHT: return -4;
        case SDLK_z: return '5';
        case SDLK_x: return '2';
        case SDLK_c: return -2;
        case SDLK_v: return '*';
        case SDLK_RETURN: return -7;
        case SDLK_BACKSPACE: return '#';
        case SDLK_s: return -6;
        case SDLK_e: return '*';
        case SDLK_r: return '#';
        default: break;
    }
    if (k >= SDLK_0 && k <= SDLK_9) return '0' + (k - SDLK_0);
    if (k >= SDLK_KP_1 && k <= SDLK_KP_9) return '1' + (k - SDLK_KP_1);
    if (k == SDLK_KP_0) return '0';
    return 0;
}

// --- Guion de pruebas (variable de entorno GOWNX_GUION), para probar sin manos:
//   "3000:foto:a.bmp;4000:tecla:-7;9000:salir"   (tiempos en ms desde el arranque)
// "tecla" pulsa y suelta 120 ms despues.
typedef struct { Uint32 ms; int tipo; int valor; char ruta[200]; int hecho; } Paso;
static Paso guion[128];
static int npasos = -1;
static int soltar_tecla;
static Uint32 soltar_ms;
static Uint32 frames_total, frames_t0, frames_n0;
static double cpu_t0;

// Tiempo de CPU del proceso entero, en ms (en Windows clock() es tiempo de reloj, no sirve).
#ifdef _WIN32
#include <windows.h>
static double cpu_ms(void) {
    FILETIME c, e, k, u;
    GetProcessTimes(GetCurrentProcess(), &c, &e, &k, &u);
    unsigned long long t = (((unsigned long long) k.dwHighDateTime << 32) | k.dwLowDateTime)
                         + (((unsigned long long) u.dwHighDateTime << 32) | u.dwLowDateTime);
    return t / 10000.0;
}
#else
static double cpu_ms(void) { return clock() * 1000.0 / CLOCKS_PER_SEC; }
#endif

static void guion_carga(void) {
    npasos = 0;
    const char *g = getenv("GOWNX_GUION");
    if (!g) return;
    char *copia = strdup(g), *ctx = NULL;
    for (char *t = strtok_r(copia, ";", &ctx); t && npasos < 128; t = strtok_r(NULL, ";", &ctx)) {
        Paso *p = &guion[npasos];
        char tipo[16] = "", arg[200] = "";
        unsigned ms = 0;
        if (sscanf(t, "%u:%15[^:]:%199s", &ms, tipo, arg) < 2) continue;
        p->ms = ms;
        p->hecho = 0;
        p->tipo = !strcmp(tipo, "foto") ? 1 : !strcmp(tipo, "tecla") ? 2 : !strcmp(tipo, "salir") ? 3 : 0;
        p->valor = atoi(arg);
        strncpy(p->ruta, arg, sizeof(p->ruta) - 1);
        if (p->tipo) npasos++;
    }
    free(copia);
}

// Devuelve un evento (como poll) o 0.
static int guion_paso(void) {
    if (npasos < 0) guion_carga();
    Uint32 ahora = SDL_GetTicks();
    if (soltar_tecla && ahora >= soltar_ms) {
        int k = soltar_tecla;
        soltar_tecla = 0;
        return (2 << 16) | (k & 0xFFFF);
    }
    for (int i = 0; i < npasos; i++) {
        Paso *p = &guion[i];
        if (p->hecho || ahora < p->ms) continue;
        if (p->tipo == 2 && soltar_tecla) continue;   // espera a que se suelte la anterior
        p->hecho = 1;
        if (p->tipo == 1) {
            SDL_LockMutex(mtx_frame);
            SDL_Surface *sf = SDL_CreateRGBSurfaceWithFormatFrom(frente, scr_w, scr_h, 32, scr_w * 4, SDL_PIXELFORMAT_ARGB8888);
            SDL_SaveBMP(sf, p->ruta);
            SDL_FreeSurface(sf);
            SDL_UnlockMutex(mtx_frame);
            {
                // CPU de todo el proceso (juego + GC + este hilo) por fotograma: lo que hay que
                // multiplicar por ~5,4 para estimar la consola (ver PLAN.md, fase 1).
                static MedFoto mf;
                double cpu = cpu_ms(), pinta, ciclo;
                Uint32 nf = frames_total - frames_n0;
                unsigned long pr, ps, pt;
                med_lee(&mf, &pinta, &ciclo);
                gownx_puentes_cuenta(&pr, &ps, &pt);
                jvm_printf("gow_nx: foto %s a los %u ms: %.1f fps, %.2f ms de CPU por fotograma, pintar %.2f ms, ciclo %.2f ms"
                           " | puentes: %lu regiones, %lu sprites, %lu tiles\n",
                           p->ruta, ahora, ahora > frames_t0 ? nf * 1000.0 / (ahora - frames_t0) : 0.0,
                           nf ? (cpu - cpu_t0) / nf : 0.0, pinta, ciclo, pr, ps, pt);
                cpu_t0 = cpu;
            }
            frames_t0 = ahora;
            frames_n0 = frames_total;
        } else if (p->tipo == 2) {
            soltar_tecla = p->valor;
            soltar_ms = ahora + 120;
            return (1 << 16) | (p->valor & 0xFFFF);
        } else if (p->tipo == 3) {
            gownx_puentes_resumen();
            return -1;
        }
    }
    return 0;
}

void gownx_perfil_paso(MiniJVM *jvm, unsigned ahora_ms, unsigned frames);   // src/nat/perfil.c

static s32 n_poll(Runtime *runtime, JClass *clazz) {
    if (hay_frame) {
        SDL_LockMutex(mtx_frame);
        SDL_UpdateTexture(tex, NULL, frente, scr_w * 4);
        hay_frame = 0;
        frames_total++;
        SDL_UnlockMutex(mtx_frame);
        int ow = scr_w, oh = scr_h;
        SDL_GetRendererOutputSize(ren, &ow, &oh);
        SDL_Rect d;                                   // encajado conservando la proporcion
        if ((long long) ow * scr_h <= (long long) oh * scr_w) {
            d.w = ow;
            d.h = (int) ((long long) ow * scr_h / scr_w);
        } else {
            d.h = oh;
            d.w = (int) ((long long) oh * scr_w / scr_h);
        }
        d.x = (ow - d.w) / 2;
        d.y = (oh - d.h) / 2;
        if (tex2 && (d.w != scr_w * 2 || d.h != scr_h * 2) && d.w > scr_w) {
            SDL_SetRenderTarget(ren, tex2);
            SDL_RenderCopy(ren, tex, NULL, NULL);
            SDL_SetRenderTarget(ren, NULL);
            SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
            SDL_RenderClear(ren);
            SDL_RenderCopy(ren, tex2, NULL, &d);
        } else {
            SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
            SDL_RenderClear(ren);
            SDL_RenderCopy(ren, tex, NULL, &d);
        }
        SDL_RenderPresent(ren);
    }
    {   // registro de rendimiento cada 5 s (en la consola es la unica forma de verlo)
        static Uint32 t_ult, n_ult;
        Uint32 ahora = SDL_GetTicks();
        if (ahora - t_ult >= 5000) {
            static MedFoto mr;
            double pinta, ciclo;
            med_lee(&mr, &pinta, &ciclo);
            if (t_ult) jvm_printf("gow_nx: %u s, %.1f fps, pintar %.2f ms, ciclo %.2f ms, %d imagenes (%.1f MB)\n", ahora / 1000,
                                  (frames_total - n_ult) * 1000.0 / (ahora - t_ult), pinta, ciclo, vivas, bytes_vivos / 1048576.0);
            t_ult = ahora;
            n_ult = frames_total;
        }
    }
    gownx_perfil_paso(runtime->jvm, SDL_GetTicks(), frames_total);   // solo hace algo en tools/perfilar.sh
    SDL_Event e;
    int r = guion_paso();
    while (!r && SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            r = -1;
        } else if ((e.type == SDL_KEYDOWN && !e.key.repeat) || e.type == SDL_KEYUP) {
            int k = tecla_midp(e.key.keysym.sym);
            if (k) r = ((e.type == SDL_KEYDOWN ? 1 : 2) << 16) | (k & 0xFFFF);
        } else if (e.type == SDL_JOYBUTTONDOWN || e.type == SDL_JOYBUTTONUP) {
            int k = gownx_boton_midp(e.jbutton.button);
            if (k) r = ((e.type == SDL_JOYBUTTONDOWN ? 1 : 2) << 16) | (k & 0xFFFF);
        }
    }
    push_int(runtime->stack, r);
    return 0;
}

static java_native_method TABLA[] = {
    {"gownx/N", "init", "(III)V", n_init},
    {"gownx/N", "imgDecode", "([BII)I", n_imgDecode},
    {"gownx/N", "imgNew", "(III)I", n_imgNew},
    {"gownx/N", "imgRegion", "(IIIIII)I", n_imgRegion},
    {"gownx/N", "imgFree", "(I)V", n_imgFree},
    {"gownx/N", "imgW", "(I)I", n_imgW},
    {"gownx/N", "imgH", "(I)I", n_imgH},
    {"gownx/N", "drawImage", "(IIIIIIIII)V", n_drawImage},
    {"gownx/N", "fillRect", "(IIIIIIIIII)V", n_fillRect},
    {"gownx/N", "drawLine", "(IIIIIIIIII)V", n_drawLine},
    {"gownx/N", "drawRGB", "(I[IIIIIIIIIIII)V", n_drawRGB},
    {"gownx/N", "present", "()V", n_present},
    {"gownx/N", "mide", "(I)V", n_mide},
    {"gownx/N", "poll", "()I", n_poll},
};

// Al terminar: cerrar SDL. Si no, sus hilos (el de audio sobre todo) siguen vivos cuando el
// cargador de homebrew ya ha descargado el .nro y la consola da un "Instruction Abort".
void gownx_audio_cierra(void);
void gownx_nat_cierra(void) {
    gownx_audio_cierra();
    if (tex2) SDL_DestroyTexture(tex2);
    if (tex) SDL_DestroyTexture(tex);
    tex2 = NULL;
    if (ren) SDL_DestroyRenderer(ren);
    if (win) SDL_DestroyWindow(win);
    tex = NULL;
    ren = NULL;
    win = NULL;
    SDL_Quit();
}

void gownx_registra_nativos(MiniJVM *jvm) {
    native_reg_lib(jvm, TABLA, sizeof(TABLA) / sizeof(TABLA[0]));
}
