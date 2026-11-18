// gow_nx: configuracion de sljit para Nintendo Switch (se incluye con -DSLJIT_HAVE_CONFIG_PRE=1).
// La memoria ejecutable la da src/switch/jit_mem.c, con dos vistas (RW para escribir, RX
// para ejecutar); sljit lo maneja con SLJIT_EXEC_OFFSET.
#ifndef GOWNX_SLJIT_CONFIG_PRE_H
#define GOWNX_SLJIT_CONFIG_PRE_H

#define SLJIT_EXECUTABLE_ALLOCATOR 0
// sljit solo suma SLJIT_EXEC_OFFSET con su propio asignador "PROT"; esta marca (parche en
// sljitLir.c, SLJIT_ADD_EXEC_OFFSET) hace que lo sume tambien con el nuestro.
#define GOWNX_SLJIT_DOS_VISTAS 1

void *gownx_jit_alloc(unsigned long size);
long gownx_jit_offset(void *p);
void gownx_jit_flush(void *desde, void *hasta);

#define SLJIT_MALLOC_EXEC(size, exec_allocator_data) gownx_jit_alloc(size)
#define SLJIT_FREE_EXEC(ptr, exec_allocator_data) ((void) (ptr))   /* no se reutiliza: ver jit_mem.c */
#define SLJIT_EXEC_OFFSET(ptr) gownx_jit_offset(ptr)
#define SLJIT_CACHE_FLUSH(from, to) gownx_jit_flush((void *) (from), (void *) (to))

#endif
