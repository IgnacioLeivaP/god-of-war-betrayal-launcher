// gow_nx: memoria ejecutable para el JIT de miniJVM (sljit) en la Switch.
// Adaptado de duels_nx/src/switch/jit_memory.cpp (ver ahi la explicacion larga).
//
// Horizon no deja que una pagina se pueda escribir y ejecutar a la vez: el trozo se ve dos
// veces, una de lectura+ejecucion (RX, donde corre el codigo) y otra de lectura+escritura
// (RW, donde lo escribe sljit). sljit lo soporta de fabrica: SLJIT_MALLOC_EXEC devuelve la
// direccion RW y SLJIT_EXEC_OFFSET la distancia hasta la RX (ver compat/sljitConfigPre.h).
//
//  - En la consola (hbloader da el handle del proceso): un bloque del monton pasa a ser
//    memoria de codigo (svcMapProcessCodeMemory) y se vuelve a ver como RW (svcMapProcessMemory).
//  - Sin ese handle (Eden, que arranca el .nro directamente): svcCreateCodeMemory.
//
// Un solo trozo, repartido en orden y sin reutilizar lo liberado: los metodos compilados
// son pequenos y casi nunca se descartan. Al salir HAY que deshacerlo (gownx_jit_libera):
// si el trozo sigue mapeado como codigo, el menu de homebrew se cae al reutilizar el monton.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <switch.h>

#define TAM_TROZO (32u << 20)

static Mutex cerrojo;
static u8 *rx, *rw;
static void *monton;
static Handle mem_codigo = INVALID_HANDLE;
static size_t usado;
static int fallo;

static int con_handle_del_proceso(Handle proc) {
    virtmemLock();
    void *x = virtmemFindCodeMemory(TAM_TROZO, 0x1000);
    void *w = NULL;
    Result rc = svcMapProcessCodeMemory(proc, (u64) x, (u64) monton, TAM_TROZO);
    int mapeado = R_SUCCEEDED(rc);
    if (mapeado) rc = svcSetProcessMemoryPermission(proc, (u64) x, TAM_TROZO, Perm_Rx);
    if (R_SUCCEEDED(rc)) {
        w = virtmemFindAslr(TAM_TROZO, 0x1000);
        rc = svcMapProcessMemory(w, proc, (u64) x, TAM_TROZO);
    }
    if (R_FAILED(rc) && mapeado) svcUnmapProcessCodeMemory(proc, (u64) x, (u64) monton, TAM_TROZO);
    virtmemUnlock();
    if (R_FAILED(rc)) {
        printf("[jit] memoria de codigo del proceso: 0x%x\n", (unsigned) rc);
        return 0;
    }
    rx = (u8 *) x;
    rw = (u8 *) w;
    return 1;
}

static int con_objeto_de_codigo(void) {
    Result rc = svcCreateCodeMemory(&mem_codigo, monton, TAM_TROZO);
    if (R_FAILED(rc)) {
        printf("[jit] svcCreateCodeMemory: 0x%x\n", (unsigned) rc);
        mem_codigo = INVALID_HANDLE;
        return 0;
    }
    virtmemLock();
    void *w = virtmemFindAslr(TAM_TROZO, 0x1000);
    void *x = NULL;
    rc = svcControlCodeMemory(mem_codigo, CodeMapOperation_MapOwner, w, TAM_TROZO, Perm_Rw);
    int escritura = R_SUCCEEDED(rc);
    if (escritura) {
        x = virtmemFindCodeMemory(TAM_TROZO, 0x1000);
        rc = svcControlCodeMemory(mem_codigo, CodeMapOperation_MapSlave, x, TAM_TROZO, Perm_Rx);
        if (R_FAILED(rc)) svcControlCodeMemory(mem_codigo, CodeMapOperation_UnmapOwner, w, TAM_TROZO, 0);
    }
    virtmemUnlock();
    if (R_FAILED(rc)) {
        printf("[jit] svcControlCodeMemory (%s): 0x%x\n", escritura ? "ejecucion" : "escritura", (unsigned) rc);
        svcCloseHandle(mem_codigo);
        mem_codigo = INVALID_HANDLE;
        return 0;
    }
    rx = (u8 *) x;
    rw = (u8 *) w;
    return 1;
}

static int prepara(void) {
    if (rx) return 1;
    if (fallo) return 0;
    monton = memalign(0x1000, TAM_TROZO);
    if (!monton) {
        printf("[jit] no hay %u MB libres para el codigo compilado\n", TAM_TROZO >> 20);
        fallo = 1;
        return 0;
    }
    Handle proc = envGetOwnProcessHandle();
    int ok = proc != INVALID_HANDLE ? con_handle_del_proceso(proc) : con_objeto_de_codigo();
    if (!ok) {
        free(monton);
        monton = NULL;
        fallo = 1;
        return 0;
    }
    printf("[jit] memoria de codigo: %u MB (%s)\n", TAM_TROZO >> 20, proc != INVALID_HANDLE ? "handle del proceso" : "objeto de codigo");
    return 1;
}

// Direccion RW de un bloque nuevo, o NULL (sljit lo toma como "sin memoria" y el metodo
// se queda en el interprete).
void *gownx_jit_alloc(unsigned long size) {
    void *p = NULL;
    mutexLock(&cerrojo);
    size = (size + 15) & ~15UL;
    if (prepara() && usado + size <= TAM_TROZO) {
        p = rw + usado;
        usado += size;
    }
    mutexUnlock(&cerrojo);
    return p;
}

// Distancia de la vista RW a la RX (la misma para todo el trozo).
long gownx_jit_offset(void *p) {
    (void) p;
    return rx ? (long) (rx - rw) : 0;
}

// sljit avisa con direcciones RX de lo que acaba de generar: lo escrito por la vista RW
// tiene que llegar a memoria y la cache de instrucciones olvidarlo.
void gownx_jit_flush(void *desde, void *hasta) {
    if (!rx) return;
    size_t n = (size_t) ((u8 *) hasta - (u8 *) desde);
    armDCacheClean(rw + ((u8 *) desde - rx), n);
    armICacheInvalidate(desde, n);
}

// Al salir, sin ningun hilo ejecutando codigo compilado.
void gownx_jit_libera(void) {
    if (!rx) return;
    Result rc;
    if (mem_codigo != INVALID_HANDLE) {
        rc = svcControlCodeMemory(mem_codigo, CodeMapOperation_UnmapSlave, rx, TAM_TROZO, 0);
        if (R_SUCCEEDED(rc)) rc = svcControlCodeMemory(mem_codigo, CodeMapOperation_UnmapOwner, rw, TAM_TROZO, 0);
        if (R_SUCCEEDED(rc)) svcCloseHandle(mem_codigo);
    } else {
        Handle proc = envGetOwnProcessHandle();
        rc = svcUnmapProcessMemory(rw, proc, (u64) rx, TAM_TROZO);
        if (R_SUCCEEDED(rc)) rc = svcUnmapProcessCodeMemory(proc, (u64) rx, (u64) monton, TAM_TROZO);
    }
    if (R_FAILED(rc)) {
        printf("[jit] no se pudo deshacer la memoria de codigo: 0x%x\n", (unsigned) rc);
    } else {
        free(monton);
        printf("[jit] memoria de codigo devuelta (%u KB usados)\n", (unsigned) (usado >> 10));
    }
    rx = rw = NULL;
    monton = NULL;
}
