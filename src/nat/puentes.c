// gow_nx: "puentes" — metodos del juego que se ejecutan en C en vez de su bytecode.
//
// Idea tomada del port fan de PS2 («God of War Betrayal Remastered»), que hace lo mismo con
// GOW_NATIVE_B_TILES/SPRITE/SETUP_BRIDGE sobre las clases b (capas de tiles) y c (sprites).
// El .jar del juego no cambia: miniJVM llama a gownx_puente() al cargar cada clase
// (external/miniJVM: class_load.c) y, si aqui se devuelve una funcion, el metodo pasa a
// comportarse como un nativo ya resuelto.
//
// Solo se puentean metodos que dibujan y no escriben campos; cada uno es una traduccion
// linea a linea del Java de ../work/ws146/src y deja el Graphics como lo encontro (el Java
// guarda y restaura el recorte). Antes de activar uno se comprueba que la clase tenga los
// campos esperados con su tipo: con otro .jar no se activa y el juego sigue en Java.
//
// Se apagan con GOWNX_PUENTES=0 (PC) o con un fichero vacio "sin_puentes" junto al .nro.
// GOWNX_PUENTES=comparar (solo para pruebas, muy lento): cada llamada se hace dos veces, en C
// y con el bytecode original, sobre el mismo buffer, y se comparan pixel a pixel.
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "jvm.h"
#include "jvm_util.h"

void gownx_blit(int dst, int src, int x, int y, int cx, int cy, int cw, int ch, int flip);   // nat.c
int gownx_img_px(int id, uint32_t **px, int *w, int *h);                                     // nat.c

#define ARG_I(n) localvar_getInt(runtime->localvar, (n))
#define ARG_R(n) ((Instance *) localvar_getRefer(runtime->localvar, (n)))
#define F_I(o, off) (*(s32 *) (instance_fields(o) + (off)))
#define F_S(o, off) (*(s16 *) (instance_fields(o) + (off)))
#define F_Z(o, off) (*(s8 *) (instance_fields(o) + (off)))
#define F_R(o, off) (*(Instance **) (instance_fields(o) + (off)))

static FieldInfo *busca_campo(JClass *cl, const char *nombre, const char *firma) {
    for (; cl; cl = cl->superclass) {
        for (s32 i = 0; i < cl->fieldPool.field_used; i++) {
            FieldInfo *fi = &cl->fieldPool.field[i];
            if (!(fi->access_flags & ACC_STATIC) && fi->name && utf8_equals_c(fi->name, nombre)
                && (!firma || utf8_equals_c(fi->descriptor, firma)))
                return fi;
        }
    }
    return NULL;
}

// Desplazamiento de un campo de instancia (se calcula al preparar la clase, despues de cargarla:
// por eso se busca en la primera llamada, no al instalar el puente).
static int off(JClass *cl, const char *nombre) {
    FieldInfo *fi = busca_campo(cl, nombre, NULL);
    if (!fi) {
        jvm_printf("gow_nx: puente sin el campo %s.%s\n", utf8_cstr(cl->name), nombre);
        exit(1);   // no deberia pasar: puente_elige() ya comprobo que existe
    }
    return fi->offset_instance;
}

// --- Graphics e Image de la capa de celular (gow_nx/java/javax/microedition/lcdui)
static int g_ok, g_dst, g_w, g_h, g_tx, g_ty, g_cx, g_cy, g_cw, g_ch, i_id, i_w, i_h;

static void prepara_g(Instance *g, Instance *img) {
    if (g_ok) return;
    JClass *cg = g->mb.clazz;
    g_dst = off(cg, "dst");
    g_w = off(cg, "w");
    g_h = off(cg, "h");
    g_tx = off(cg, "tx");
    g_ty = off(cg, "ty");
    g_cx = off(cg, "cx");
    g_cy = off(cg, "cy");
    g_cw = off(cg, "cw");
    g_ch = off(cg, "ch");
    JClass *ci = img->mb.clazz;
    i_id = off(ci, "id");
    i_w = off(ci, "w");
    i_h = off(ci, "h");
    g_ok = 1;
}

static unsigned long cuenta[3];

// a.a(Image img, sx, sy, w, h, Graphics g, dx, dy) — static (a.java:609). Copia la region
// (sx, sy, w, h) de img en (dx, dy). b.c() la usa para volcar la capa a la pantalla.
//   save clip; g.clipRect(dx, dy, w, h); g.drawImage(img, dx - sx, dy - sy, 20); restore clip
static s32 p_a_region(Runtime *runtime, JClass *clazz) {
    Instance *img = ARG_R(0), *g = ARG_R(5);
    int sx = ARG_I(1), sy = ARG_I(2), w = ARG_I(3), h = ARG_I(4), dx = ARG_I(6), dy = ARG_I(7);
    cuenta[0]++;
    if (!g || !img) return 0;
    prepara_g(g, img);
    int X = dx + F_I(g, g_tx), Y = dy + F_I(g, g_ty);
    int cx = F_I(g, g_cx), cy = F_I(g, g_cy);
    int x1 = X > cx ? X : cx, y1 = Y > cy ? Y : cy;
    int x2 = X + w < cx + F_I(g, g_cw) ? X + w : cx + F_I(g, g_cw);
    int y2 = Y + h < cy + F_I(g, g_ch) ? Y + h : cy + F_I(g, g_ch);
    if (x2 <= x1 || y2 <= y1) return 0;
    gownx_blit(F_I(g, g_dst), F_I(img, i_id), X - sx, Y - sy, x1, y1, x2 - x1, y2 - y1, 0);
    return 0;
}

// --- c: un cuadro de sprite (c.java)
static int c_ok, c_l, c_a, c_b, c_c, c_d, c_i, c_j;

// c.a(Graphics g, x, y, f) — f: bit 1 = espejo vertical, bit 2 = horizontal. Recorta al
// rectangulo del cuadro (a, b, c, d) y dibuja la imagen l desplazada por (i, j). El Java
// quita la traslacion del Graphics cuando hay espejo y la devuelve al final: en coordenadas
// absolutas da lo mismo en los dos casos.
static s32 p_c_a(Runtime *runtime, JClass *clazz) {
    Instance *self = ARG_R(0), *g = ARG_R(1);
    int x = ARG_I(2), y = ARG_I(3), f = ARG_I(4);
    cuenta[1]++;
    if (!self || !g) return 0;
    if (!c_ok) {
        JClass *cc = self->mb.clazz;
        c_l = off(cc, "l");
        c_a = off(cc, "a");
        c_b = off(cc, "b");
        c_c = off(cc, "c");
        c_d = off(cc, "d");
        c_i = off(cc, "i");
        c_j = off(cc, "j");
        c_ok = 1;
    }
    Instance *img = F_R(self, c_l);
    if (!img) return 0;
    prepara_g(g, img);
    int a = F_S(self, c_a), b = F_S(self, c_b), cw = F_S(self, c_c), chh = F_S(self, c_d);
    x += (f & 2) == 0 ? a : -(a + cw);
    y += (f & 1) == 0 ? b : -(b + chh);
    int X = x + F_I(g, g_tx), Y = y + F_I(g, g_ty);
    // g.clipRect(x, y, c, d)
    int gx = F_I(g, g_cx), gy = F_I(g, g_cy);
    int x1 = X > gx ? X : gx, y1 = Y > gy ? Y : gy;
    int x2 = X + cw < gx + F_I(g, g_cw) ? X + cw : gx + F_I(g, g_cw);
    int y2 = Y + chh < gy + F_I(g, g_ch) ? Y + chh : gy + F_I(g, g_ch);
    if (x2 <= x1 || y2 <= y1) return 0;
    int i = F_S(self, c_i), j = F_S(self, c_j);
    X += (f & 2) == 0 ? -i : i + cw - F_I(img, i_w);
    Y += (f & 1) == 0 ? -j : j + chh - F_I(img, i_h);
    // f != 0 va por DirectGraphics: FLIP_HORIZONTAL si (f & 2), FLIP_VERTICAL si (f & 1)
    gownx_blit(F_I(g, g_dst), F_I(img, i_id), X, Y, x1, y1, x2 - x1, y2 - y1, ((f & 2) ? 1 : 0) | ((f & 1) ? 2 : 0));
    return 0;
}

// --- b: capa de tiles (b.java)
static int b_ok, b_l, b_k, b_q, b_r, b_m, b_n, b_o, b_p;

// b.b(Graphics g, x, y, dx, dy, w, h) — b.java:218. Pinta los tiles del mapa l que caen en
// el rectangulo (x, y, w, h) del mundo, en (dx, dy) del Graphics. Cada tile se dibuja con su
// propio recorte (setClip REEMPLAZA el recorte, como en el Java) y al final el recorte vuelve
// a ser el de antes, asi que aqui no se toca el Graphics. Los nombres vN siguen al Java.
static s32 p_b_tiles(Runtime *runtime, JClass *clazz) {
    Instance *self = ARG_R(0), *g = ARG_R(1);
    int v2 = ARG_I(2), v3 = ARG_I(3), v4 = ARG_I(4), v5 = ARG_I(5), v6 = ARG_I(6), v7 = ARG_I(7);
    cuenta[2]++;
    if (!self || !g) return 0;
    if (!b_ok) {
        JClass *cb = self->mb.clazz;
        b_l = off(cb, "l");
        b_k = off(cb, "k");
        b_q = off(cb, "q");
        b_r = off(cb, "r");
        b_m = off(cb, "m");
        b_n = off(cb, "n");
        b_o = off(cb, "o");
        b_p = off(cb, "p");
        b_ok = 1;
    }
    Instance *la = F_R(self, b_l), *n = F_R(self, b_n), *oa = F_R(self, b_o), *pa = F_R(self, b_p);
    int q = F_I(self, b_q), r = F_I(self, b_r), k = F_I(self, b_k);
    if (!la || q <= 0 || r <= 0) return 0;
    if (F_Z(self, b_m) || !n || !oa || !pa) return 0;   // m: capa sin imagen (el Java no dibuja nada)
    prepara_g(g, n);
    const s16 *l = (const s16 *) jarray_body(la);
    const u16 *o = (const u16 *) jarray_body(oa), *p = (const u16 *) jarray_body(pa);
    int nl = jarray_length(la), no = jarray_length(oa), np = jarray_length(pa);
    int dst = F_I(g, g_dst), nid = F_I(n, i_id), tx = F_I(g, g_tx), ty = F_I(g, g_ty);
    int gw = F_I(g, g_w), gh = F_I(g, g_h);   // limites del buffer (setClip recorta contra ellos)

    int v12 = (v2 + v6 - 1) / q;
    int v13 = v12 - v2 / q;
    int v14 = (v3 + v7 - 1) / r;
    v12 += v14 * k;
    v14 -= v3 / r;
    v2 %= q;
    v3 %= r;
    int v15 = v2, v16 = v3;
    int v17 = -(v2 + v6) % q;
    if (v17 < 0) v17 += q;
    int v18 = -(v3 + v7) % r;
    if (v18 < 0) v18 += r;
    v2 = v4 - v2 + q * v13;
    v3 = v5 - v3 + r * v14;

    for (int v29 = v14; v29 >= 0; v29--) {
        for (int v28 = v13; v28 >= 0; v28--) {
            int idx = v12--;
            if (idx < 0 || idx >= nl) return 0;   // el Java lanzaria ArrayIndexOutOfBounds
            int v19 = l[idx] & 4095;
            if (v19 > 0) {
                v19--;
                if (v19 >= no || v19 >= np) return 0;
                int a4 = v28 == 0 ? v15 : 0, a20 = v28 == v13 ? v17 : 0;
                int a5 = v29 == 0 ? v16 : 0, a21 = v29 == v14 ? v18 : 0;
                // g.setClip(v2 + a4, v3 + a5, q - a4 - a20, r - a5 - a21)
                int x1 = v2 + a4 + tx, y1 = v3 + a5 + ty;
                int x2 = x1 + (q - a4 - a20), y2 = y1 + (r - a5 - a21);
                if (x1 < 0) x1 = 0;
                if (y1 < 0) y1 = 0;
                if (x2 > gw) x2 = gw;
                if (y2 > gh) y2 = gh;
                // g.drawImage(n, v2 - o[v19], v3 - p[v19], 20)
                if (x2 > x1 && y2 > y1)
                    gownx_blit(dst, nid, v2 - o[v19] + tx, v3 - p[v19] + ty, x1, y1, x2 - x1, y2 - y1, 0);
            }
            v2 -= q;
        }
        v12 += 1 + v13 - k;
        v2 += q * (v13 + 1);
        v3 -= r;
    }
    return 0;
}

// --- instalacion

typedef struct { const char *nombre, *firma; } Campo;

static int tiene(JClass *cl, const Campo *c) {
    for (; c->nombre; c++)
        if (!busca_campo(cl, c->nombre, c->firma)) return 0;
    return 1;
}

static const Campo CAMPOS_C[] = {
    {"l", "Ljavax/microedition/lcdui/Image;"}, {"a", "S"}, {"b", "S"}, {"c", "S"}, {"d", "S"}, {"i", "S"}, {"j", "S"}, {NULL, NULL}};
static const Campo CAMPOS_B[] = {
    {"l", "[S"}, {"k", "I"}, {"q", "I"}, {"r", "I"}, {"m", "Z"},
    {"n", "Ljavax/microedition/lcdui/Image;"}, {"o", "[C"}, {"p", "[C"}, {NULL, NULL}};
static const Campo SIN_CAMPOS[] = {{NULL, NULL}};

typedef struct {
    const char *clase, *metodo, *firma;
    const Campo *campos;
    int estatico;
    int arg_g;          // posicion del Graphics entre los argumentos (contando this)
    java_native_fun f;
} Puente;

static const Puente PUENTES[] = {
    {"a", "a", "(Ljavax/microedition/lcdui/Image;IIIILjavax/microedition/lcdui/Graphics;II)V", SIN_CAMPOS, 1, 5, p_a_region},
    {"c", "a", "(Ljavax/microedition/lcdui/Graphics;III)V", CAMPOS_C, 0, 1, p_c_a},
    {"b", "b", "(Ljavax/microedition/lcdui/Graphics;IIIIII)V", CAMPOS_B, 0, 1, p_b_tiles},
};
#define NPUENTES ((int) (sizeof(PUENTES) / sizeof(PUENTES[0])))

// --- modo comparar
static int comparar;
static MethodInfo *metodo_de[NPUENTES];   // el MethodInfo puenteado (para saber cual se llama)
static CodeAttribute *codigo_de[NPUENTES];   // su bytecode (miniJVM se lo quita al puentearlo)
static MethodInfo *java_de[NPUENTES];     // copia sin puente: ejecuta el bytecode original
static unsigned long comparadas[NPUENTES], distintas[NPUENTES];

static s32 p_comparar(Runtime *runtime, JClass *clazz) {
    MethodInfo *m = runtime->method;
    int i = 0;
    while (i < NPUENTES && metodo_de[i] != m) i++;
    if (i == NPUENTES) {
        jvm_printf("gow_nx: comparar: metodo desconocido\n");
        exit(1);
    }
    const Puente *p = &PUENTES[i];
    Instance *g = ARG_R(p->arg_g);
    if (!g) return p->f(runtime, clazz);
    if (!java_de[i]) {   // en la primera llamada la clase ya esta preparada del todo
        java_de[i] = (MethodInfo *) malloc(sizeof(MethodInfo));
        *java_de[i] = *m;
        java_de[i]->is_native = 0;
        java_de[i]->native_func = NULL;
        java_de[i]->converted_code = codigo_de[i];
    }
    uint32_t *px;
    int w, h;
    JClass *cg = g->mb.clazz;
    int dst = F_I(g, off(cg, "dst"));
    if (!gownx_img_px(dst, &px, &w, &h)) return p->f(runtime, clazz);
    size_t n = (size_t) w * h;
    uint32_t *antes = (uint32_t *) malloc(n * 4), *en_c = (uint32_t *) malloc(n * 4);
    memcpy(antes, px, n * 4);
    int ga[6] = {F_I(g, off(cg, "tx")), F_I(g, off(cg, "ty")), F_I(g, off(cg, "cx")), F_I(g, off(cg, "cy")), F_I(g, off(cg, "cw")), F_I(g, off(cg, "ch"))};

    p->f(runtime, clazz);                 // 1) en C
    memcpy(en_c, px, n * 4);
    memcpy(px, antes, n * 4);

    MethodInfo *mj = java_de[i];          // 2) el bytecode original, con los mismos argumentos
    int slot = 0;
    if (!mj->is_static) push_ref(runtime->stack, localvar_getRefer(runtime->localvar, slot++));
    for (int k = 0; k < mj->paraType->length; k++, slot++) {
        if (utf8_char_at(mj->paraType, k) == 'R') push_ref(runtime->stack, localvar_getRefer(runtime->localvar, slot));
        else push_int(runtime->stack, localvar_getInt(runtime->localvar, slot));
    }
    s32 ret = execute_method_impl(mj, runtime);
    if (ret != RUNTIME_STATUS_NORMAL) {   // p. ej. el hilo se detiene al salir: el Java quedo a medias
        free(antes);
        free(en_c);
        return ret;
    }

    int gd[6] = {F_I(g, off(cg, "tx")), F_I(g, off(cg, "ty")), F_I(g, off(cg, "cx")), F_I(g, off(cg, "cy")), F_I(g, off(cg, "cw")), F_I(g, off(cg, "ch"))};
    size_t dif = 0;
    int x0 = w, y0 = h, x1 = -1, y1 = -1;
    for (size_t k = 0; k < n; k++) {
        if (en_c[k] != px[k]) {
            int x = (int) (k % w), y = (int) (k / w);
            dif++;
            if (x < x0) x0 = x;
            if (y < y0) y0 = y;
            if (x > x1) x1 = x;
            if (y > y1) y1 = y;
        }
    }
    comparadas[i]++;
    if (dif || memcmp(ga, gd, sizeof(ga))) {
        distintas[i]++;
        if (distintas[i] <= 10)
            jvm_printf("gow_nx: comparar %s.%s: %zu pixeles distintos (x %d-%d, y %d-%d)%s\n", p->clase, p->metodo, dif, x0, x1, y0, y1,
                       memcmp(ga, gd, sizeof(ga)) ? " y el Graphics cambio" : "");
    }
    free(antes);
    free(en_c);
    return ret;
}

void gownx_puentes_resumen(void) {
    if (!comparar) return;
    for (int i = 0; i < NPUENTES; i++)
        jvm_printf("gow_nx: comparar %s.%s: %lu llamadas, %lu distintas\n", PUENTES[i].clase, PUENTES[i].metodo, comparadas[i], distintas[i]);
}

static java_native_fun puente_elige(JClass *cl, MethodInfo *m) {
    if (!cl->name || cl->name->length > 1) return NULL;   // las clases del juego se llaman a..e
    for (int i = 0; i < NPUENTES; i++) {
        const Puente *p = &PUENTES[i];
        if (utf8_equals_c(cl->name, p->clase) && utf8_equals_c(m->name, p->metodo) && utf8_equals_c(m->descriptor, p->firma)
            && m->is_static == p->estatico && !m->is_sync) {
            if (!tiene(cl, p->campos)) {
                jvm_printf("gow_nx: puente %s.%s NO instalado (la clase no tiene los campos esperados)\n", p->clase, p->metodo);
                return NULL;
            }
            jvm_printf("gow_nx: puente %s.%s%s\n", p->clase, p->metodo, p->firma);
            metodo_de[i] = m;
            codigo_de[i] = m->converted_code;
            return comparar ? p_comparar : p->f;
        }
    }
    return NULL;
}

int gownx_puentes_activos;

// Antes de jvm_init (las clases del juego se cargan despues, al arrancar el MIDlet).
void gownx_puentes_inicia(void) {
    const char *e = getenv("GOWNX_PUENTES");
    FILE *f = fopen("sin_puentes", "rb");
    gownx_puentes_activos = !(e && e[0] == '0') && !f;
    comparar = e && !strcmp(e, "comparar");
    if (f) fclose(f);
    gownx_puente = gownx_puentes_activos ? puente_elige : NULL;
    jvm_printf("gow_nx: puentes nativos %s\n", !gownx_puentes_activos ? "apagados" : comparar ? "en modo comparar" : "activos");
}

// Llamadas a cada puente desde el arranque (para comprobar que se usan, tambien con el JIT).
void gownx_puentes_cuenta(unsigned long *region, unsigned long *sprite, unsigned long *tiles) {
    *region = cuenta[0];
    *sprite = cuenta[1];
    *tiles = cuenta[2];
}
