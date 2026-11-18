// gow_nx: sustituto de <dlfcn.h> para Switch (no hay carga dinamica; los nativos van enlazados).
#ifndef GOWNX_DLFCN_H
#define GOWNX_DLFCN_H
#define RTLD_LAZY 1
#define RTLD_NOW 2
#ifdef __cplusplus
extern "C" {
#endif
void *dlopen(const char *name, int mode);
void *dlsym(void *lib, const char *sym);
int dlclose(void *lib);
const char *dlerror(void);
#ifdef __cplusplus
}
#endif
#endif
