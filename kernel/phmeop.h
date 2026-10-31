#ifndef PHMEOP_H
#define PHMEOP_H

/*
 * phmeop: definiciones compartidas entre el driver de kernel y su cliente
 * en espacio de usuario (APK / JNI).
 *
 * IMPORTANTE: estas estructuras y números de ioctl deben ser idénticos,
 * byte a byte, en ambos lados. No cambies el orden ni los tipos.
 */

#ifdef __KERNEL__
#include <linux/types.h>   /* pid_t, uintptr_t, size_t */
#else
#include <sys/types.h>     /* pid_t, size_t */
#include <stdint.h>        /* uintptr_t */
#include <stddef.h>        /* size_t */
#endif

#define OP_READ_MEM          0x801
#define OP_WRITE_MEM         0x802
#define OP_MODULE_BASE       0x803
#define OP_MODULE_DATA_RANGE 0x804  /* añadido: rango escribible del módulo */

struct CopyMemory {
    pid_t       pid;      /* PID del proceso objetivo */
    uintptr_t   addr;     /* dirección en el proceso objetivo */
    void       *buffer;   /* buffer en espacio de usuario (entrada/salida) */
    size_t      size;     /* número de bytes */
};

struct ModuleBase {
    pid_t       pid;      /* PID del proceso objetivo */
    char       *name;     /* nombre del módulo a buscar, p. ej. "libgame.so" */
    uintptr_t   base;     /* [salida] dirección base del módulo */
};

/*
 * OP_MODULE_DATA_RANGE: unión de los mappings ESCRIBIBLES del módulo.
 * Sirve para escanear variables globales (.data/.bss) sin depender de
 * /proc/<pid>/maps. start/end son [salida].
 */
struct ModuleRange {
    pid_t       pid;      /* PID del proceso objetivo */
    char       *name;     /* nombre del módulo, p. ej. "libUE4.so" */
    uintptr_t   start;    /* [salida] inicio del primer mapping escribible */
    uintptr_t   end;      /* [salida] fin del último mapping escribible */
};

#endif /* PHMEOP_H */
