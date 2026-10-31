# phmeop — driver de kernel `/dev/phmeop`

Driver de dispositivo de caracteres que permite a un cliente con privilegios
(root) leer/escribir la memoria de otro proceso y localizar la dirección base
de un módulo (`.so`) cargado en ese proceso.

Este módulo es **independiente** de la app principal del repositorio: se
compila contra el kernel Linux del dispositivo, no con Gradle/CMake.

## Archivos

| Archivo | Descripción |
|---|---|
| `phmeop.h` | Structs y números de ioctl compartidos (kernel y userspace) |
| `entry.c` | Implementación del driver (registro, open/release, ioctl) |
| `Makefile` | Compilación del módulo (desktop y cross-compile Android) |
| `apk_native.c` | Cliente JNI de referencia (cómo la APK habla con el driver) |

## Contrato (debe coincidir byte a byte)

```c
#define OP_READ_MEM          0x801
#define OP_WRITE_MEM         0x802
#define OP_MODULE_BASE       0x803
#define OP_MODULE_DATA_RANGE 0x804

struct CopyMemory  { pid_t pid; uintptr_t addr; void *buffer; size_t size; };
struct ModuleBase  { pid_t pid; char *name; uintptr_t base; };
struct ModuleRange { pid_t pid; char *name; uintptr_t start; uintptr_t end; };
```

- `OP_READ_MEM`: copia `size` bytes desde `addr` del proceso `pid` a `buffer`.
- `OP_WRITE_MEM`: copia `size` bytes desde `buffer` a `addr` del proceso `pid`.
- `OP_MODULE_BASE`: busca un mapping cuyo nombre de archivo contenga `name`
  y devuelve su dirección base en `base` (primera coincidencia, la más baja).
- `OP_MODULE_DATA_RANGE`: devuelve en `start`/`end` la unión de los mappings
  **escribibles** del módulo (`.data`/`.bss`). Evita depender de
  `/proc/<pid>/maps`, que puede estar restringido por SELinux.

`OP_MODULE_DATA_RANGE` es aditivo: los clientes antiguos (que solo usan
0x801–0x803) siguen funcionando sin cambios.

## Lecturas parciales

`OP_READ_MEM`/`OP_WRITE_MEM` devuelven el **número de bytes copiados**. Si una
página intermedia no está mapeada, el ioctl devuelve lo ya copiado en vez de
descartarlo; solo es negativo (`-EFAULT`) si no copió ningún byte. Un cliente
que necesite una lectura completa debe comparar con `size`.

## Reglas

- Solo **un cliente a la vez**: un segundo `open()` devuelve `-EBUSY`.
- Abrir como **root**: `open()` devuelve `-EPERM` para no root.
- El `ioctl` debe hacerse desde código **nativo** (C/C++/JNI), no desde
  Java/Kotlin directo.

## Compilación

### Linux de escritorio

```sh
cd kernel
make            # usa /lib/modules/$(uname -r)/build
```

### Android (cross-compile)

Necesitas el árbol de fuentes del kernel del dispositivo, configurado y
compilado (con `.config`), y un toolchain. Ejemplo arm64:

```sh
cd kernel
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-android- KDIR=/ruta/al/kernel/android
```

> El `.ko` debe compilarse contra la **misma** versión y configuración del
> kernel del dispositivo, o `insmod` fallará (vermagic / símbolos).

## Instalación y uso

```sh
# Como root en el dispositivo:
insmod phmeop.ko            # devtmpfs crea /dev/phmeop automáticamente
ls -l /dev/phmeop
rmmod phmeop                # descargar
```

Desde la APK (nativo):

```c
int fd = open("/dev/phmeop", O_RDWR);   // -1: errno (-EPERM, -EBUSY, ...)

struct CopyMemory cm = { .pid = pid, .addr = addr, .buffer = buf, .size = n };
ioctl(fd, OP_READ_MEM, &cm);            // lectura
ioctl(fd, OP_WRITE_MEM, &cm);           // escritura

struct ModuleBase mb = { .pid = pid, .name = "libgame.so", .base = 0 };
ioctl(fd, OP_MODULE_BASE, &mb);         // mb.base = dirección base

struct ModuleRange mr = { .pid = pid, .name = "libUE4.so", .start = 0, .end = 0 };
ioctl(fd, OP_MODULE_DATA_RANGE, &mr);   // .data/.bss escribible del módulo

close(fd);
```

Ver `apk_native.c` para el envoltorio JNI completo.

## THP (Transparent Huge Pages)

`get_user_pages_remote()` devuelve la **cabecera** del compound page (THP de
2 MB). Antes, `kmap(page)` mapeaba solo el primer 4 KB y leer/escribir un
sub-page distinto devolvía memoria equivocada. Arreglado en `entry.c`:
`phmeop_subpage()` selecciona el sub-page correcto (`compound_order`) antes
del `kmap`. Con esto el driver lee/escribe correctamente tanto páginas de
4 KB como zonas THP de 2 MB, sin recompilar el kernel.

## Build exacto (Samsung 5.10.209)

Para el `.ko` que coincide byte a byte con el kernel del dispositivo (evita
vermagic/símbolos y cubre THP real):

```sh
# 1) Fuente del kernel Samsung del dispositivo + .config (de /proc/config.gz
#    del teléfono) ya configurado y compilado.
# 2) Toolchain aarch64:
cd kernel
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-android- KDIR=/ruta/al/kernel-samsung-5.10.209
# 3) En el dispositivo (root):
adb push phmeop.ko /data/local/tmp/
adb shell "su -c 'insmod /data/local/tmp/phmeop.ko'"
adb shell "su -c 'ls -l /dev/phmeop'"
```

## Notas y limitaciones

- **32-bit sobre kernel 64-bit**: si la APK es de 32 bits (armeabi-v7a) y el
  kernel es de 64 bits, los structs cambian de tamaño (punteros de 4 vs 8
  bytes) y hace falta `compat_ioctl` con `struct compat_*`. Esta versión cubre
  el caso nativo (APK arm64 sobre kernel arm64). Usa `arm64-v8a`.
- **Páginas no mapeadas / protegidas**: se usa `FOLL_FORCE`, pero una dirección
  sin mapear devuelve `-EFAULT`. Las escrituras sobre páginas de solo lectura
  compartidas (texto, archivos mmap) no persisten; usa direcciones de datos.
  Si el fallo ocurre a mitad de una operación, el ioctl devuelve los bytes
  copiados hasta ahí (lectura parcial), no un error que descarte el trabajo.
- **Seguridad**: es un primitivo muy potente (acceso arbitrario a memoria de
  procesos). Úsalo solo en dispositivos de tu propiedad y con autorización.
- No se compila ni prueba en este entorno (Windows, sin kernel Linux): requiere
  compilar en la máquina/dispositivo de destino.
