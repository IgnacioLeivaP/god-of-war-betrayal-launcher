// gow_nx en el PC: gow_nx.exe [-int] [clase [ancho alto escala]]
//   -int   interprete puro (sin JIT), para estimar el rendimiento de la consola
// Se ejecuta desde gow_nx/sd/switch/gow_nx (misma disposicion que en la SD).
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int gownx_run(const char *bootcp, const char *cp, const char *gc, int interprete, const char *clase, int nargs, char **args);

int main(int argc, char **argv) {
    int interprete = 0, i = 1;
    if (argc > 1 && strcmp(argv[1], "-int") == 0) {
        interprete = 1;
        i = 2;
    }
    setvbuf(stdout, NULL, _IONBF, 0);
    // GOWNX_GC=malloc: el mismo recolector que usa la Switch (por defecto, immix).
    // GOWNX_CP: otro classpath (para pruebas sueltas de la capa, sin el juego).
    const char *cp = getenv("GOWNX_CP") ? getenv("GOWNX_CP") : "gownx.jar;juego.jar";
    return gownx_run("minijvm_rt.jar", cp, getenv("GOWNX_GC"), interprete, "gownx/Main", argc - i, argv + i);
}
