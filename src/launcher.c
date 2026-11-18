// gow_nx: arranque comun (PC y Switch) de miniJVM con los nativos del juego registrados.
// Sustituye al main.c de miniJVM, que no deja registrar nativos antes de llamar a main().
#include <stdio.h>
#include <string.h>
#include <locale.h>

#include "jvm.h"
#include "jvm_util.h"
#include "garbage.h"

void gownx_registra_nativos(MiniJVM *jvm);
void gownx_registra_audio(MiniJVM *jvm);
void gownx_nat_cierra(void);
void gownx_puentes_inicia(void);

// interprete != 0: sin JIT (asi se mide en el PC lo que rendiria la consola).
int gownx_run(const char *bootcp, const char *cp, const char *gc, int interprete, const char *clase, int nargs, char **args) {
    setlocale(LC_ALL, "");
    jvm_init_mem_alloc();
    ArrayList *para = arraylist_create(0);
    for (int i = 0; i < nargs; i++) arraylist_push_back(para, args[i]);

    int ret = -1;
    gownx_puentes_inicia();   // src/nat/puentes.c: antes de cargar ninguna clase del juego
    MiniJVM *jvm = jvm_create();
    if (jvm != NULL) {
        jvm->startup_dir = utf8_create_c("./");
        jvm->jdwp_enable = interprete ? 1 : 0;   // en miniJVM "depuracion" = interprete puro
        jvm->gc_backend = (c8 *) gc;
        jvm->jdwp_suspend_on_start = 0;
        jvm->max_heap_size = MAX_HEAP_SIZE_DEFAULT;
        jvm->max_vm_memory = 0;
        ret = jvm_init(jvm, (c8 *) bootcp, (c8 *) cp);
        if (ret) {
            jvm_printf("[ERROR]minijvm init error.\n");
        } else {
            gownx_registra_nativos(jvm);
            gownx_registra_audio(jvm);
            ret = call_main(jvm, (c8 *) clase, para);
        }
        if (jvm->collector->exit_flag) ret = jvm->collector->exit_code;
        jvm_destroy(jvm);
        gownx_nat_cierra();
    }
    arraylist_destroy(para);
    fflush(stdout);
    fflush(stderr);
    return ret;
}
