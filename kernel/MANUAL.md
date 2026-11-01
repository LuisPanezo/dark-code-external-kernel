# MANUAL — phmeop para entenderlo todo (y añadir teléfonos nuevos)

> Este documento es para que **tú** entiendas el proyecto de punta a punta, sin
> asumir que ya sabes de kernels. Léelo en orden. Al final hay una receta fija
> para añadir un teléfono nuevo (que es lo que harás constantemente).

---

## 1. Qué es esto, en una frase

Un **driver de kernel** que crea una "puerta" (`/dev/phmeop`) para que una app
con root pueda **leer y escribir la memoria de otro proceso** (por ejemplo un
juego) y **localizar dónde vive su código** (la dirección base de un `.so`).

En otras palabras: es el "enchufe" que conecta tu app con el cerebro del
teléfono, para poder tocar la memoria de otras apps.

---

## 2. Las dos mitades del proyecto

Todo sistema tiene capas. Este proyecto toca **dos capas**:

```
        ┌─────────────────────────────┐
        │  TU APP (Android / Java)     │  ← usa phmeop.h + apk_native.c
        │  código nativo (JNI/C)       │
        ├─────────────────────────────┤
        │  /dev/phmeop  (la "puerta")  │  ← la crea el driver
        ├─────────────────────────────┤
        │  KERNEL (el cerebro)         │  ← aquí vive phmeop.ko (entry.c)
        └─────────────────────────────┘
```

- **Mitad de arriba (app):** pide "léeme 100 bytes de este proceso" o
  "dame la base de libgame.so".
- **Mitad de abajo (kernel):** el driver hace el trabajo sucio (acceder a la
  memoria de otro proceso es algo que solo el kernel puede hacer).

El "contrato" entre ambas mitades (los números de operación y las estructuras)
está en **`phmeop.h`**, y debe ser **idéntico** en los dos lados.

---

## 3. Las 4 operaciones que sabe hacer el driver

| Operación | Qué hace | Para qué la usas |
|---|---|---|
| `OP_READ_MEM` | Leer N bytes de la memoria de otro proceso | Ver valores del juego |
| `OP_WRITE_MEM` | Escribir N bytes en la memoria de otro proceso | Modificar valores |
| `OP_MODULE_BASE` | Devolver la dirección base de un `.so` | Saber dónde empieza libgame.so |
| `OP_MODULE_DATA_RANGE` | Devolver el rango escribible (`.data`/`.bss`) de un `.so` | Escanear variables globales |

---

## 4. El problema que complica todo: GKI esconde funciones

Este es el concepto más importante, y la razón por la que el proyecto tiene
piezas "raras" como `resolve.c`.

### Qué es un "símbolo"

Dentro del kernel, cada función y variable tiene un **nombre interno** llamado
"símbolo". Ejemplos:

```
get_user_pages_remote   ← función que trae una página de memoria de un proceso
get_task_mm             ← función que devuelve el "espacio de memoria" de un proceso
d_path                  ← función que devuelve la ruta de un archivo mapeado
vmem_map                ← variable (datos) que apunta al mapa de memoria física
```

Para leer/escribir la memoria de otro proceso, el driver **necesita** usar esas
funciones internas.

### Qué hace GKI

Desde Android 12, casi todos los teléfonos usan **GKI** (Generic Kernel Image):
un kernel estandarizado por Google. Pero Google, para proteger el sistema, solo
deja **"exportados"** (disponibles para módulos externos) un puñado de símbolos
de una lista oficial llamada **KMI**. Las funciones internas que nosotros
necesitamos (`get_user_pages_remote`, `get_task_mm`, `d_path`…) **NO están en
esa lista**.

### Consecuencia

Si el driver las llama directamente, el teléfono dice:

```
insmod: ERROR: could not insert module: Unknown symbol get_user_pages_remote
```

### Solución (lo que hace `resolve.c`)

En lugar de llamarlas directamente, el driver **las busca en tiempo de carga**:

1. El kernel guarda una "guía telefónica" interna llamada **kallsyms** que
   traduce nombre → dirección.
2. La función que consulta esa guía (`kallsyms_lookup_name`) **también está
   escondida**. Así que `resolve.c` la localiza primero con un truco
   (camina la tabla de símbolos usando `sprintf("%pS")`).
3. Una vez localizada, la usa para encontrar la dirección de todas las demás
   funciones escondidas.
4. Las llama mediante **punteros de función** con el tipo exacto (necesario
   porque el kernel tiene activada una protección llamada CFI/KCFI).

**Resumen en una frase:** GKI esconde las funciones → `resolve.c` las encuentra
en tiempo de carga → `entry.c` las usa.

---

## 5. Mapa de archivos (qué hace cada uno)

```
kernel/
├── entry.c           El driver en sí: crea /dev/phmeop y hace las 4 operaciones.
│                     Contiene la lógica de leer/escribir memoria y buscar .so.
├── resolve.c         El "buscador de símbolos escondidos" (solo se usa en GKI).
├── resolve.h         La cabecera de resolve.c.
├── phmeop.h          El contrato kernel ↔ app (números de ioctl + estructuras).
│                     Este archivo se COPIA a la app (debe ser idéntico).
├── apk_native.c      Ejemplo de cómo la app habla con el driver (JNI).
│                     Es referencia, no se compila dentro del .ko.
├── Makefile          Cómo se compila. Tiene el "interruptor" GKI / no-GKI.
├── check_symbols.sh  Script que verifica si un kernel es compatible.
├── BUILD_GKI.md      Guía técnica paso a paso de compilación.
└── MANUAL.md         Este documento (la explicación completa).
```

### Qué archivo tocas tú cuando añades un teléfono

| Situación | Archivo que tocas |
|---|---|
| Añadir un teléfono **GKI** (lo normal) | **Ninguno** del código. Solo cambias la ruta del kernel al compilar (`KDIR`). |
| Añadir un teléfono **no-GKI** | Ninguno; compilas con `PHMEOP_DIRECT=1`. |
| Cambiar qué hace el driver (nueva operación) | `entry.c` + `phmeop.h` |
| El driver no encuentra un símbolo en un kernel raro | `resolve.c` (o la lista de `phmeop_resolve_all` en `entry.c`) |

**Idea clave:** añadir un teléfono nuevo **casi nunca** es escribir código
nuevo. Es **recompilar el mismo código contra el kernel de ese teléfono**.

---

## 6. El ciclo de vida (los 5 pasos de siempre)

```
 1. ESCRIBIR   (ya está hecho: entry.c + resolve.c)
        ↓
 2. COMPILAR   (make contra el kernel EXACTO del teléfono)
        ↓
 3. SUBIR      (adb push phmeop.ko /data/local/tmp/)
        ↓
 4. CARGAR     (su -c 'insmod /data/local/tmp/phmeop.ko')
        ↓
 5. USAR       (la app abre /dev/phmeop y hace ioctls)
```

Los pasos 1 ya están terminados para tu RedMagic. Los pasos 2–5 se repiten por
cada teléfono nuevo.

---

## 7. Glosario mínimo (léelo una vez y listo)

| Término | Qué significa, en cristiano |
|---|---|
| **Kernel** | El "cerebro" del sistema operativo. Controla hardware y memoria. Todo corre encima. |
| **Módulo (.ko)** | Un "plugin" que se inyecta al kernel sin modificarlo. `phmeop.ko` es nuestro plugin. |
| **Driver** | Un módulo que crea una "puerta" (`/dev/...`) para que las apps lo usen. |
| **GKI** | Kernel estándar de Google (Android 12+). Esconde funciones internas. |
| **Símbolo** | El nombre interno de una función/variable del kernel. |
| **Exportar** | Dejar un símbolo "disponible" para los módulos. GKI exporta pocos. |
| **kallsyms** | La "guía telefónica" interna: nombre ↔ dirección. |
| **vermagic** | La "huella digital" del kernel. El `.ko` debe coincidir o no carga. |
| **root** | Permisos de superusuario. Sin root no puedes cargar módulos. |
| **insmod / rmmod** | Cargar / descargar el módulo en memoria. |
| **/dev/phmeop** | La "puerta" que crea el driver; la app la abre. |
| **ioctl** | El "mensaje" que la app le manda al driver para pedirle algo. |
| **KMI** | La lista corta de símbolos que GKI SÍ deja disponibles. |
| **CFI/KCFI** | Protección del kernel que exige llamar las funciones con el tipo exacto. |

---

## 8. RECETA: cómo añadir un teléfono nuevo (siempre igual)

Copia este bloque y sigue los pasos. Es lo que harás cada vez.

### Paso A — Conecta y confirma root

```powershell
.\adb.exe shell "su -c 'id'"
```

Debe decir `uid=0(root)`.

### Paso B — Saca los datos del teléfono

```powershell
.\adb.exe shell "su -c 'uname -r'"
.\adb.exe shell "su -c 'zcat /proc/config.gz | grep -E \"CONFIG_MODULES|CONFIG_KALLSYMS|CONFIG_ARM64_4K_PAGES|CONFIG_ARM64_16K_PAGES\"'"
```

Anota:
- `uname -r` (ej: `6.6.56-android15-8-g38447e018c92-ab12829524-4k`)
- Si tiene `CONFIG_MODULES=y`, `CONFIG_KALLSYMS_ALL=y`, `CONFIG_ARM64_4K_PAGES=y`.

### Paso C — Verifica las 5 condiciones

1. ✅ Root
2. ✅ arm64
3. ✅ `CONFIG_MODULES=y`
4. ✅ Páginas 4K
5. ✅ `CONFIG_KALLSYMS_ALL=y`  ← si falta, avísame: se ajusta el truco de `kmap`

### Paso D — Consigue el kernel EXACTO

- **Si es GKI** (la mayoría): clona la rama `common-androidXX-Y.Y` de Google y
  haz checkout al commit que aparece en `uname -r` después del `-g`:
  - `6.6.56-...-g38447e018c92-...` → commit = `38447e018c92`
  - Y el build ID (lo que va tras el `-ab`): `12829524` → para bajar el
    `Module.symvers`/headers precompilados de ci.android.com.
- **Si es no-GKI** (Xiaomi/HyperOS, etc.): necesitas el código fuente del
  fabricante (más difícil; suelen publicarlo o estar en GitHub).

### Paso E — Compila

```bash
cd kernel

# GKI:
make ARCH=arm64 LLVM=1 LLVM_IAS=1 KDIR=/ruta/al/kernel/de/ese/telefono

# no-GKI (si el fabricante exporta los símbolos):
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-android- KDIR=/ruta/al/kernel PHMEOP_DIRECT=1
```

Sale `phmeop.ko` **para ese teléfono**.

### Paso F — Sube, carga y verifica

```powershell
.\adb.exe push phmeop.ko /data/local/tmp/
.\adb.exe shell "su -c 'insmod /data/local/tmp/phmeop.ko'"
.\adb.exe shell "su -c 'ls -l /dev/phmeop'"
.\adb.exe shell "su -c 'dmesg | grep phmeop'"
```

Si ves `/dev/phmeop` y en `dmesg` sale `phmeop: /dev/phmeop registrado`, listo.

---

## 9. Qué hago cuando algo falla (guía rápida)

| Error al cargar | Significa | Solución |
|---|---|---|
| `Unknown symbol X` | Ese símbolo no está exportado y no se resolvió | Dime cuál es; ajustamos `resolve.c` o la lista de `phmeop_resolve_all` |
| `Invalid module format` / vermagic | Compilaste contra otro kernel | Recompila contra el kernel EXACTO (mismo commit/build) |
| `Required key not available` | El kernel exige firma | Tu RedMagic no la exige (`MODULE_SIG_FORCE` off); en otros, insmod -f |
| Carga pero `dmesg` dice "no se resolvió vmem_map" | El kernel no tiene `KALLSYMS_ALL` | Cambiamos el truco de `kmap` (te ayudo) |
| Carga pero no aparece `/dev/phmeop` | `misc_register` falló | Mira `dmesg` y pásame la línea |

---

## 10. En resumen (para que no te pierdas)

1. **Qué es:** un driver que deja a una app con root leer/escribir memoria de
   otros procesos y localizar módulos `.so`.
2. **Por qué es "complicado":** GKI esconde las funciones internas; `resolve.c`
   las encuentra en tiempo de carga.
3. **Para qué teléfono:** se compila UN `.ko` por teléfono. El código se
   reutiliza; el `.ko` no se copia.
4. **Añadir un teléfono =** sacar su `uname -r` → conseguir su kernel →
   `make` → `push` + `insmod`. Casi nunca se toca el código C.
