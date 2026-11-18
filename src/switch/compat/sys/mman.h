// gow_nx: sustituto de <sys/mman.h> para Nintendo Switch (libnx no tiene mmap).
// Solo cubre lo que usa miniJVM (immix.c): reservas anonimas sobre el heap de newlib.
#ifndef GOWNX_SYS_MMAN_H
#define GOWNX_SYS_MMAN_H
#include <stddef.h>
#include <sys/types.h>
#ifdef __cplusplus
extern "C" {
#endif
#define PROT_NONE 0
#define PROT_READ 1
#define PROT_WRITE 2
#define PROT_EXEC 4
#define MAP_PRIVATE 2
#define MAP_ANONYMOUS 0x20
#define MAP_ANON MAP_ANONYMOUS
#define MAP_NORESERVE 0x4000
#define MAP_FAILED ((void *) -1)
#define MADV_DONTNEED 4
void *mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off);
int munmap(void *addr, size_t len);
int mprotect(void *addr, size_t len, int prot);
int madvise(void *addr, size_t len, int advice);
#ifdef __cplusplus
}
#endif
#endif
