# Compilar y cargar `phmeop.ko` en el RedMagic 10 Pro (GKI android15-6.6)

Este driver usa símbolos del kernel que GKI **no exporta**, por eso está
refactorizado para resolverlos en runtime (`resolve.c`) y llamarlos con
punteros de función tipados (CFI-safe). Aquí está el proceso completo, desde
cero, en WSL2/Linux.

## 0. Datos de tu teléfono (para que el `.ko` coincida)

- `uname -r`: `6.6.56-android15-8-g38447e018c92-ab12829524-4k`
- Rama GKI: `common-android15-6.6`, commit `38447e018c92`, build `12829524`
- Toolchain: Clang `r510928` (viene en `prebuilts/clang/host/linux-x86/clang-r510928`)

## 1. Requisitos de la máquina de build (WSL2 recomendado)

- ~40 GB de disco libre (el árbol GKI pesa mucho).
- `repo`, `git`, `make`, `bison`, `flex`, `libssl-dev`, `python3`.
- Ubuntu 22.04/24.04 en WSL2.

## 2. Bajar el árbol GKI y el toolchain

```bash
mkdir -p ~/gki && cd ~/gki
repo init -u https://android.googlesource.com/kernel/manifest \
          -b common-android15-6.6 --depth=1
repo sync -c -j$(nproc)

# Fijar el commit EXACTO de tu build (38447e018c92)
cd common
git fetch --depth=1 https://android.googlesource.com/kernel/common 38447e018c92
git checkout 38447e018c92
```

> El clang r510928 se descarga con el manifest (en `prebuilts/clang/host/linux-x86/clang-r510928`).

## 3. Configurar y compilar el kernel (genera `Module.symvers` y headers)

```bash
cd ~/gki/common
./build/build.sh            # o: BUILD_CONFIG=common/build.config.gki.aarch64 build/build.sh
```

Esto tarda (horas la primera vez). Al terminar tendrás:

- `out/android15-6.6/dist/Module.symvers`
- `out/android15-6.6/dist/` con `vmlinux`, `System.map`, headers.

Para un build más ligero de solo lo necesario a un módulo externo, también
puedes compilar el target `virtual_device_aarch64`:

```bash
tools/bazel run //common:virtual_device_aarch64_dist
```

## 4. Verificar símbolos (paso barato y decisivo)

Baja el `Module.symvers`/`abi_symbollist` de tu build exacto desde
[ci.android.com](https://ci.android.com) (busca `12829524`) y ejecuta:

```bash
./check_symbols.sh Module.symvers
```

Debes ver `OK` en todos los de "REFERENCIA DIRECTA". Los de "SE RESUELVEN EN
RUNTIME" deben aparecer como no exportados (por eso el driver los resuelve).

## 5. Compilar el módulo

```bash
cd <ruta/a/este/repo>/kernel

# GKI / Android arm64:
make ARCH=arm64 LLVM=1 LLVM_IAS=1 \
     KDIR=~/gki/common/out/virtual_device_aarch64/common
```

> Si tu árbol está en `out/android15-6.6/dist`, apunta `KDIR` ahí. El punto es
> que el directorio tenga el `.config`/`Makefile` del kernel y `Module.symvers`.

Sale `phmeop.ko` en este directorio.

## 6. Subirlo y cargarlo en el teléfono (KernelSU)

```powershell
.\adb.exe push phmeop.ko /data/local/tmp/
.\adb.exe shell "su -c 'insmod /data/local/tmp/phmeop.ko'"
.\adb.exe shell "su -c 'ls -l /dev/phmeop'"
.\adb.exe shell "su -c 'dmesg | grep phmeop'"
```

Si `insmod` falla:

- **`Unknown symbol ...`** → ese símbolo está referenciado directamente pero no
  está exportado. Verifica con `check_symbols.sh`. Normalmente significa que
  `sprintf` (punto de partida del resolver) no está en la KMI de tu build.
- **`Invalid module format` / vermagic** → compilaste contra un kernel distinto;
  vuelve al commit/build exacto.
- Si el módulo carga, verás en `dmesg`:
  ```
  phmeop: kallsyms_lookup_name en 0x...
  phmeop: /dev/phmeop registrado
  ```

## 7. Carga automática al arrancar (opcional)

Módulo KernelSU en `/data/adb/modules/phmeop/`:

```bash
# post-fs-data.sh
#!/system/bin/sh
insmod "${0%/*}/phmeop.ko"
```

con `phmeop.ko` junto al script.

## Notas de seguridad / limitaciones

- Es un primitivo muy potente (acceso arbitrario a memoria de procesos).
  Úsalo solo en dispositivos de tu propiedad y con autorización.
- El resolver asume `CONFIG_KALLSYMS_ALL=y` (tu kernel lo tiene) para poder
  resolver el símbolo de **datos** `vmem_map`. Si no estuviera, `kmap` no
  funcionaría en arm64 y habría que cambiar la estrategia.
- `PHMEOP_DIRECT=1` solo sirve para probar la lógica en un kernel de escritorio
  (donde todos los símbolos están exportados); **no** lo uses para el teléfono.
