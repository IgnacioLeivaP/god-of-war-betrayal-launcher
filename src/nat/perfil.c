// gow_nx: perfil por metodo de miniJVM, solo en la compilacion de perfilado
// (tools/perfilar.sh, que compila miniJVM con _JVM_DEBUG_METHOD_PROFILE = 1).
// GOWNX_PERFIL="desde:hasta" (ms desde el arranque): a "desde" pone los contadores a cero y a
// "hasta" imprime los metodos con mas tiempo. El tiempo es inclusivo (cuenta lo que hacen los
// metodos que llama) y el propio perfilado encarece las llamadas cortas: sirve para ordenar,
// no para sumar.
#include <stdlib.h>
#include <stdio.h>
#include "jvm.h"
#include "jvm_util.h"

#if _JVM_DEBUG_METHOD_PROFILE
static int perfil_compara(ArrayListValue a, ArrayListValue b) {
    MethodInfo *m1 = (MethodInfo *) a, *m2 = (MethodInfo *) b;
    return m2->profile_total_time > m1->profile_total_time ? 1 : m2->profile_total_time < m1->profile_total_time ? -1 : 0;
}

void gownx_perfil_paso(MiniJVM *jvm, unsigned ahora_ms, unsigned frames) {
    static int estado = -1;
    static unsigned desde, hasta, frames0;
    if (estado < 0) {
        const char *e = getenv("GOWNX_PERFIL");
        estado = (e && sscanf(e, "%u:%u", &desde, &hasta) == 2) ? 0 : 2;
    }
    if (estado == 0 && ahora_ms >= desde) {
        profile_method_reset(jvm);
        frames0 = frames;
        estado = 1;
    } else if (estado == 1 && ahora_ms >= hasta) {
        estado = 2;
        unsigned nf = frames - frames0;
        ArrayList *todos = arraylist_create(1024);
        profile_method_get_all(jvm, todos);
        arraylist_sort(todos, perfil_compara);
        jvm_printf("--- perfil %u-%u ms, %u fotogramas (us por fotograma, inclusivo) ---\n", desde, hasta, nf);
        for (int i = 0; i < 60 && i < todos->length; i++) {
            MethodInfo *m = (MethodInfo *) arraylist_get_value(todos, i);
            jvm_printf("%9.1f us/fot %9.2f llamadas/fot  %s.%s%s\n",
                       nf ? m->profile_total_time / 1000.0 / nf : 0.0, nf ? (double) m->profile_count / nf : 0.0,
                       utf8_cstr(m->_this_class->name), utf8_cstr(m->name), utf8_cstr(m->descriptor));
        }
        arraylist_destroy(todos);
    }
}
#else
void gownx_perfil_paso(MiniJVM *jvm, unsigned ahora_ms, unsigned frames) {
    (void) jvm; (void) ahora_ms; (void) frames;
}
#endif
