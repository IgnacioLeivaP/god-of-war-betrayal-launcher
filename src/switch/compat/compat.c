// gow_nx: funciones del sistema que miniJVM espera y la Switch no tiene.
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <dlfcn.h>

// --- memoria: "mmap anonimo" sobre el heap. Las paginas salen a cero, como espera el GC.
void *mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off) {
    (void) addr; (void) prot; (void) flags; (void) fd; (void) off;
    void *p = memalign(0x1000, len);
    if (!p) { errno = ENOMEM; return MAP_FAILED; }
    memset(p, 0, len);
    return p;
}
int munmap(void *addr, size_t len) { (void) len; free(addr); return 0; }
int mprotect(void *addr, size_t len, int prot) { (void) addr; (void) len; (void) prot; return 0; }
int madvise(void *addr, size_t len, int advice) {
    if (advice == MADV_DONTNEED) memset(addr, 0, len);   // Linux devuelve paginas a cero
    return 0;
}

// --- sin carga dinamica ni procesos
void *dlopen(const char *name, int mode) { (void) name; (void) mode; return NULL; }
void *dlsym(void *lib, const char *sym) { (void) lib; (void) sym; return NULL; }
int dlclose(void *lib) { (void) lib; return 0; }
const char *dlerror(void) { return "sin carga dinamica en Switch"; }
pid_t waitpid(pid_t pid, int *status, int options) { (void) pid; (void) status; (void) options; errno = ECHILD; return -1; }

// --- reloj C11 que newlib no trae
#include <time.h>
int timespec_get(struct timespec *ts, int base) {
    if (clock_gettime(CLOCK_REALTIME, ts) != 0) return 0;
    return base;
}

// --- sin procesos hijos
int pipe(int fd[2]) { (void) fd; errno = ENOSYS; return -1; }
int execvp(const char *file, char *const argv[]) { (void) file; (void) argv; errno = ENOSYS; return -1; }

// --- sin red: el juego no la usa, y asi no hay que arrastrar mbedtls
#define SIN_RED(nombre) int nombre(void) { return -1; }
SIN_RED(mbedtls_net_send) SIN_RED(mbedtls_net_recv) SIN_RED(mbedtls_net_set_nonblock)
SIN_RED(mbedtls_net_init) SIN_RED(mbedtls_net_free) SIN_RED(mbedtls_net_bind)
SIN_RED(mbedtls_net_accept) SIN_RED(mbedtls_net_usleep) SIN_RED(mbedtls_net_set_block)
SIN_RED(mbedtls_net_recv_timeout) SIN_RED(mbedtls_net_connect)
SIN_RED(sslc_write) SIN_RED(sslc_wrap) SIN_RED(sslc_read) SIN_RED(sslc_init)
SIN_RED(sslc_connect) SIN_RED(sslc_close)
