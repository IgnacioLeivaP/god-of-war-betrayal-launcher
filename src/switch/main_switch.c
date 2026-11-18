// gow_nx: arranque en Nintendo Switch. Los ficheros van en sdmc:/switch/gow_nx/:
//   gow_nx.nro  minijvm_rt.jar  gownx.jar  juego.jar   (+ rms/ con las partidas, gow_nx.log)
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <switch.h>

void gownx_jit_libera(void);
int gownx_run(const char *bootcp, const char *cp, const char *gc, int interprete, const char *clase, int nargs, char **args);

#define DIR_APP "sdmc:/switch/gow_nx"

int main(int argc, char **argv) {
    (void) argc; (void) argv;
    chdir(DIR_APP);
    // El registro va a un fichero: en Eden y en la consola no hay terminal que mirar.
    freopen(DIR_APP "/gow_nx.log", "w", stdout);
    setvbuf(stdout, NULL, _IONBF, 0);
    dup2(fileno(stdout), fileno(stderr));
    printf("gow_nx: arranque\n");

    // Rutas relativas: el separador de classpath en POSIX es ':' y chocaria con "sdmc:".
    // GC "malloc" (el immix reserva 1 GB con mmap).
    u64 t0 = armGetSystemTick();
    // Con "interprete" en la SD (un fichero vacio con ese nombre) se arranca sin JIT, por si falla.
    FILE *f = fopen("interprete", "rb");
    int interprete = f != NULL;
    if (f) fclose(f);
    printf("gow_nx: %s\n", interprete ? "interprete puro" : "con JIT");
    int r = gownx_run("minijvm_rt.jar", "gownx.jar:juego.jar", "malloc", interprete, "gownx/Main", 0, NULL);
    gownx_jit_libera();
    u64 ms = armTicksToNs(armGetSystemTick() - t0) / 1000000ULL;
    printf("gow_nx: miniJVM termino con %d tras %llu ms\n", r, (unsigned long long) ms);
    fflush(stdout);
    return 0;
}
