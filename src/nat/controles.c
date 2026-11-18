// gow_nx: reparto de botones del mando -> teclas del celular, configurable con un fichero
// de texto (controles.txt, junto al .nro) para ajustarlo sin recompilar.
//
// Numeracion de botones: la de switch-sdl2 (A=0, B=1, X=2, Y=3, sticks pulsados 4/5, L=6,
// R=7, ZL=8, ZR=9, +=10, -=11, cruceta 12..15, stick izq. como botones 16..19, der. 20..23).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define NBOTONES 24

// The retail MIDlet expects phone keypad codes and has separate soft keys.
#define T_SALTO_ATRAS '2'
#define T_ATAQUE_OK   '5'
static int mapa[NBOTONES] = {
    T_ATAQUE_OK,    // A
    T_SALTO_ATRAS,  // B
    -6,             // X      (sin uso pedido: queda como tecla blanda izquierda, de reserva)
    T_ATAQUE_OK,    // Y      duplicado de A
    0, 0,           // sticks pulsados
    -2,             // L      bloquear / interactuar (abajo)
    '*',            // R      cambiar arma
    0, 0,           // ZL, ZR
    -7,             // +      pausa (tecla blanda derecha)
    '#',            // -      menu Power Up
    -3, -1, -4, -2,   // D-pad: left, up, right, down
    -3, -1, -4, -2,   // stick izquierdo
    0, 0, 0, 0,       // stick derecho
};
#define VERSION_FICHERO "controles retail v1"

static const struct { const char *nombre; int boton; } BOTONES[] = {
    {"A", 0}, {"B", 1}, {"X", 2}, {"Y", 3}, {"LSTICK", 4}, {"RSTICK", 5}, {"L", 6}, {"R", 7},
    {"ZL", 8}, {"ZR", 9}, {"MAS", 10}, {"MENOS", 11},
    {"IZQUIERDA", 12}, {"ARRIBA", 13}, {"DERECHA", 14}, {"ABAJO", 15},
};

static const struct { const char *nombre; int tecla; } TECLAS[] = {
    {"arriba", -1}, {"abajo", -2}, {"izquierda", -3}, {"derecha", -4}, {"fuego", -5},
    {"blanda_izq", -6}, {"blanda_der", -7}, {"asterisco", '*'}, {"numeral", '#'}, {"nada", 0},
    {"ataque_ok", T_ATAQUE_OK}, {"salto_atras", T_SALTO_ATRAS},
    {"0", '0'}, {"1", '1'}, {"2", '2'}, {"3", '3'}, {"4", '4'},
    {"5", '5'}, {"6", '6'}, {"7", '7'}, {"8", '8'}, {"9", '9'},
};

static const char *nombre_tecla(int t) {
    for (unsigned i = 0; i < sizeof(TECLAS) / sizeof(TECLAS[0]); i++) if (TECLAS[i].tecla == t) return TECLAS[i].nombre;
    return "nada";
}

static char *recorta(char *s) {
    while (*s && isspace((unsigned char) *s)) s++;
    char *fin = s + strlen(s);
    while (fin > s && isspace((unsigned char) fin[-1])) *--fin = 0;
    return s;
}

static void escribe_por_defecto(const char *ruta) {
    FILE *f = fopen(ruta, "w");
    if (!f) return;
    fputs("# gow_nx: " VERSION_FICHERO ". Format: BUTTON = phone key. Read at startup.\n", f);
    fputs("# Delete this file to restore defaults. Keep the first line when editing.\n", f);
    fputs("# Buttons: A B X Y L R ZL ZR MAS MENOS LSTICK RSTICK ARRIBA ABAJO IZQUIERDA DERECHA\n", f);
    fputs("# Directions apply to both the D-pad and the left stick.\n", f);
    fputs("# Keys: arriba abajo izquierda derecha fuego blanda_izq blanda_der asterisco numeral 0..9 nada\n", f);
    fputs("# ataque_ok = phone 5 (attack/confirm); salto_atras = phone 2 (up/jump).\n", f);
    fputs("# blanda_izq and blanda_der are the left and right soft keys.\n\n", f);
    for (unsigned i = 0; i < sizeof(BOTONES) / sizeof(BOTONES[0]); i++) {
        fprintf(f, "%s = %s\n", BOTONES[i].nombre, nombre_tecla(mapa[BOTONES[i].boton]));
    }
    fclose(f);
}

void gownx_controles_carga(const char *ruta) {
    FILE *f = fopen(ruta, "r");
    if (!f) {
        escribe_por_defecto(ruta);
        return;
    }
    char linea[200];
    // Un fichero de otra version (otro esquema por defecto) se reescribe con el actual.
    if (!fgets(linea, sizeof(linea), f) || !strstr(linea, VERSION_FICHERO)) {
        fclose(f);
        escribe_por_defecto(ruta);
        return;
    }
    while (fgets(linea, sizeof(linea), f)) {
        char *igual = strchr(linea, '=');
        if (linea[0] == '#' || !igual) continue;
        *igual = 0;
        char *b = recorta(linea), *t = recorta(igual + 1);
        int boton = -1, tecla = 0, hay = 0;
        for (unsigned i = 0; i < sizeof(BOTONES) / sizeof(BOTONES[0]); i++) if (!strcmp(BOTONES[i].nombre, b)) boton = BOTONES[i].boton;
        for (unsigned i = 0; i < sizeof(TECLAS) / sizeof(TECLAS[0]); i++) {
            if (!strcmp(TECLAS[i].nombre, t)) {
                tecla = TECLAS[i].tecla;
                hay = 1;
            }
        }
        if (boton < 0 || !hay) {
            printf("gow_nx: controles.txt: no entiendo \"%s = %s\"\n", b, t);
            continue;
        }
        mapa[boton] = tecla;
        if (boton >= 12 && boton <= 15) mapa[boton + 4] = tecla;   // la cruceta arrastra al stick izquierdo
    }
    fclose(f);
}

int gownx_boton_midp(int boton) {
    return boton >= 0 && boton < NBOTONES ? mapa[boton] : 0;
}
