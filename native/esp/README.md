# Módulo ESP — lector de Delta Force

Lee el estado de la partida desde el proceso del juego a través del driver
`/dev/phmeop` y entrega **rectángulos de pantalla listos para dibujar**.
No dibuja nada por sí mismo: el overlay (ImGui u otro) sólo consume datos.

```text
/dev/phmeop ──► IMemoryDriver ──► EspReader ──► FrameSnapshot (canónico, metros)
                                       │            └─► motor de escena (opcional)
                                       │
                                       └─► Targets (cm de Unreal) ──► Projector ──► DcEspBox (px)
                                                                          ▲
                                                            EspService (hilo) ──► dc_esp_poll()
```

## Qué hace y qué no

| Sí | No |
|---|---|
| Lee UWorld → nivel → actores → posición, vida, equipo y nombre | No escribe memoria del juego |
| Escanea `GWorld` en los datos RW de `libUE4.so` | No evade anti-cheat ni ofusca nada |
| Proyecta con la matriz del motor de escena | No captura pantalla ni usa accesibilidad |
| Un hilo de adquisición; el hilo de dibujo no hace ioctl | No pide root: lo exige el driver al abrir |

## Archivos

| Archivo | Responsabilidad |
|---|---|
| `abi.h` | Incluye `kernel/phmeop.h` (contrato único) + `static_assert` del ABI |
| `offsets.h` | Todos los offsets confirmados. Si un parche los cambia, se toca sólo aquí |
| `types.h` / `types.cpp` | `FEncVector`, `Options`, `Target`, `Stats`, estados |
| `driver.h` | `IMemoryDriver`, `IProcessMap` y helpers tipados |
| `driver_linux.cpp` | Cliente real del driver (ioctl). En escritorio compila a stubs |
| `process_linux.cpp` | `/proc/<pid>/cmdline` y `/proc/<pid>/maps` |
| `scanner.h/.cpp` | Validación de `UWorld` y escaneo de la variable global |
| `ue.h/.cpp` | Rotador → base, FOV, cadena del jugador, vida/equipo/nombre, `FEncVector` |
| `reader.h/.cpp` | `EspReader`: produce el `FrameSnapshot` que ya consume el motor |
| `projector.h/.cpp` | AABB canónico → rectángulo en píxeles (misma matriz que la escena) |
| `api/esp_api.h` | API C para el overlay (`dc_esp_*`) |
| `api/esp_service.h/.cpp` | Hilo de adquisición, publicación atómica, interruptor |
| `esp_jni.cpp` + `EspNative.java` | Puente mínimo para encender/apagar desde Java |
| `tests/` | Pruebas con memoria simulada (sin dispositivo) |
| `tool/main.cpp` | `esp_df`: probe / test / dump / loop |

## Permisos y reglas del driver

- El driver exige **root** (`CAP_SYS_ADMIN`) en `open()`. Un `esp_df` lanzado
  con `su -c` cumple; la APK necesita ejecutarse como root.
- **Un solo cliente a la vez**: si la APK tiene el dispositivo abierto,
  `esp_df` falla con `EBUSY` (y viceversa). Cierra uno antes de abrir el otro.
- El cliente sólo usa `OP_READ_MEM`, `OP_MODULE_BASE` y `OP_MODULE_DATA_RANGE`.

## Vector cifrado (`bEncrypted`)

El `FVector` de esta build ocupa 16 bytes: `x,y,z` + `index` (u16) + `bEncrypted`.
La política es conservadora y explícita:

1. `Codec::Auto` usa el valor tal cual si es creíble (suele ser el caso incluso
   con el flag puesto: son los mismos floats con bits bajos recortados).
2. Si no es creíble, prueba a restaurar los 16 bits bajos de `X` desde `index`
   (hipótesis documentada). Si tampoco, el actor se descarta y se cuenta.
3. `esp_df dump --raw` imprime `raw=(%08x %08x %08x) index=N enc=K` por actor:
   con eso se puede reversear el `DecVector` real y fijar `Codec::Trailer`
   o añadir un códec nuevo en `ue.cpp` sin tocar el resto.

Nada de esto "adivina" un esquema desconocido en silencio: si no hay forma
creíble de leer la posición, el objetivo no se dibuja y el contador sube.

## Compilar

### Herramienta de diagnóstico (binario suelto, lo normal)

```sh
cd native/esp/tool
NDK=~/android-ndk-r28 ./build_android.sh          # arm64-v8a
NDK=~/android-ndk-r28 ABI=x86_64 ./build_android.sh
```

O a mano, en cualquier sistema:

```sh
cmake -S native -B build-esp-arm64 \
  -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-29 \
  -DCMAKE_BUILD_TYPE=Release -DDARKCODE_ESP_TOOL=ON
cmake --build build-esp-arm64 --target esp_df
```

### APK

Nada especial: `esp_core` ya está en `native/CMakeLists.txt` y
`libdarkcode_scene.so` incluye `esp_jni.cpp` y la API `dc_esp_*`.
La herramienta `esp_df` está apagada por defecto (`DARKCODE_ESP_TOOL=OFF`),
así que el build de Gradle no cambia de comportamiento.

### Driver

Sólo si quieres `OP_MODULE_DATA_RANGE` (evita depender de `/proc`):

```sh
cd kernel && make ARCH=arm64 CROSS_COMPILE=aarch64-linux-android- KDIR=/ruta/al/kernel
```

Sin ese ioctl el cliente sigue funcionando como root leyendo `/proc/<pid>/maps`.
El `.ko` debe compilarse contra el kernel exacto del dispositivo (vermagic).

## Ejecutar

```sh
adb push build-esp-arm64/esp_df /data/local/tmp/
adb shell "su -c chmod 755 /data/local/tmp/esp_df"
adb shell "su -c /data/local/tmp/esp_df probe"     # driver + PID + GWorld
adb shell "su -c /data/local/tmp/esp_df test"      # 300 ticks de diagnóstico
adb shell "su -c /data/local/tmp/esp_df dump --raw" # 1 tick con bits crudos
adb shell "su -c /data/local/tmp/esp_df loop --interval 16"
```

| Salida | Significado | Qué hacer |
|---|---|---|
| `[-] EPERM: /dev/phmeop exige root` | no eres root | `su -c` |
| `[-] ENOENT: el driver no está cargado` | falta el `.ko` | `insmod phmeop.ko` |
| `[-] EBUSY` | otro cliente lo tiene abierto | cierra el otro proceso |
| `[-] Juego no encontrado` | paquete distinto | `--package` |
| `[-] GWorld no encontrado` | menú principal o escaneo fallido | entra a partida; `probe` |
| `[-] Cámara no disponible` | carga/muerte/espectador | esperar |
| `---- 0 targets ----` | actores no legibles | `dump --all`, revisar offsets |
| `[ENC!]` en muchas cajas | posición cifrada | `dump --raw` y reversear |
| `BOX ...` con valores sanos | ✅ funcionando | conectar el overlay |

Opciones útiles: `--screen 2400x1080`, `--fov 90`, `--fov-vertical`,
`--max-dist 300`, `--codec plain|trailer|auto`, `--all`, `--gworld 0x...`
(se salta el escaneo), `--quiet`.

## Conectar el overlay (lo que falta por tu parte)

Hay un archivo listo para pegar: [`examples/overlay_imgui.cpp`](examples/overlay_imgui.cpp)
(compilado aquí contra una cabecera ImGui de prueba, sin avisos). Dibuja caja,
esquinas, barra de vida, nombre + distancia y snapline, y trae `DrawEspPanel()`
con el interruptor `op2`, el origen de la snapline (arriba/medio/abajo) y el estado.

Lo mínimo, en el hilo de dibujo:

```cpp
#include "esp/api/esp_api.h"

// Una vez, al abrir el overlay (root + driver cargado):
dc_esp_attach("com.garena.game.df", 0);

// Cada frame: cero ioctl, sólo proyección del último frame publicado.
DcEspBox boxes[64];
const int count = dc_esp_poll(io.DisplaySize.x, io.DisplaySize.y, boxes, 64);
ImDrawList* draw = ImGui::GetBackgroundDrawList();
for (int i = 0; i < count; ++i) {
    const ImVec2 top{boxes[i].x, boxes[i].y};
    const ImVec2 bottom{boxes[i].x + boxes[i].w, boxes[i].y + boxes[i].h};
    draw->AddRect(top, bottom, IM_COL32(255, 70, 70, 255), 0.f, 0, 1.5f);
    draw->AddText(ImVec2(top.x, top.y - 14.f), IM_COL32(255, 255, 255, 230), boxes[i].name);
}

// El interruptor:
dc_esp_set_enabled(on ? 1 : 0);
```

Con el ejemplo, todo eso se reduce a:

```cpp
EspOverlayOptions esp;   // esp.op2 = interruptor
DrawEspOverlay(ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y, esp);
DrawEspPanel(esp);
```

Desde Java (el switch de la UI), con `EspNative`:

```java
if (EspNative.attach("com.garena.game.df", 0) != EspNative.RESULT_OK) {
    status.setText(EspNative.status());
}
EspNative.setEnabled(true);            // interruptor
String text = EspNative.status();      // "En vivo · 7 objetivos" / motivo del fallo
```

`dc_esp_attach` **no bloquea**: arranca el hilo y devuelve. El escaneo de
`GWorld` (unos cientos de ms) ocurre dentro del hilo; `dc_esp_status()` informa.
Si el juego no está abierto, el servicio reintenta cada 2 s.

Para dibujar con el motor de escena en vez de ImGui, `dc_esp_view(&view)`
copia cámara + entidades en metros canónicos (mismo formato que
`dc::FrameSnapshot`).

## Enviar la prueba a otro teléfono / otra PC

```powershell
powershell -ExecutionPolicy Bypass -File native\esp\tool\package_test.ps1
```

Compila ambas ABI y genera `dist\esp-test\` + `dist\esp-test.zip` (≈10 MB):

| Archivo | Para qué |
|---|---|
| `esp_df-arm64` / `esp_df-x86_64` | binarios ya compilados (teléfono / emulador) |
| `ejecutar.ps1` / `ejecutar.sh` | copian el binario y lanzan probe/test/dump |
| `LEEME.md` | instrucciones para quien prueba |
| `MANIFIESTO.txt` | tamaños y SHA-256 de cada archivo |
| `platform-tools/` | adb incluido (se omite con `-NoAdb`) |

La otra persona sólo necesita un teléfono de 64 bits con root y el driver
**ya cargado** (`ls -l /dev/phmeop`), cable USB y depuración USB activa.
No necesita SDK, NDK, Gradle ni este repositorio. Devuelve `esp-diagnostico.txt`.

Un `.ko` no se puede copiar entre teléfonos: depende del kernel exacto
(vermagic). Si el otro dispositivo no lo tiene cargado, hay que compilarlo allí.

## Pruebas

```sh
cmake -S native -B native-out -DCMAKE_BUILD_TYPE=Release
cmake --build native-out
ctest --test-dir native-out --output-on-failure    # scene_contracts + esp_contracts
```

`esp_tests` monta un proceso simulado con los offsets reales (UWorld, nivel,
actores, cámara, PlayerState) y comprueba rotador → base, FOV, decodificación,
escaneo de GWorld, lectura completa, caché de actores y proyección.

## Estado de los offsets (probado en partida)

Verificado en un Galaxy S22 Ultra (SM-S908U, Android 14) dentro de una partida
de extracción, con 371 actores en el nivel:

| Dato | Offset | Estado |
|---|---|---|
| `UWorld::PersistentLevel` | `0xF8` | ✅ |
| `ULevel::Actors` | `0x98` | ✅ `num=371` |
| `AActor::RootComponent` → `RelativeLocation` | `0x180` → `0x168` | ✅ |
| `UWorld::OwningGameInstance` | `0x190` | ✅ |
| `UGameInstance::LocalPlayers` | `0x38` | ✅ (`num=1`) |
| `UPlayer::PlayerController` | `0x30` | ✅ |
| `APlayerController::AcknowledgedPawn` | `0x3F0` | ✅ (también en `0x3A0`/`0x3B0`) |
| `APlayerController::PlayerCameraManager` | `0x408` | ✅ |
| **`POV::Location`** | **`0x1890`** | ✅ **corregido** (la lista traía `0x3F0`) |
| `POV::Rotation` / `POV::FOV` | `0x18A0` / `0x18AC` | ✅ |
| `APawn::PlayerState` | `0x390` | ✅ |
| **`PlayerName` (FString)** | **`0x388`** | ✅ **corregido** (la lista traía `0x378`) |
| `ADFMCharacter::HealthDataComponent` (`+0x24E8`) → `+0x280` → `+0x3C` | — | ⚠️ da 95/100 en algunos actores, `-1` en otros (base actualizada, por reverificar) |
| `AGPCharacterBase::TeamComp` (`+0x10C0`) → `+0x108` | — | ⚠️ muchos actores quedan en `T-1` (base actualizada, por reverificar) |

Cómo se encontraron los dos corregidos (por si un parche los mueve):

```sh
# la POV se localiza por el patrón Location+Rotation+FOV, anclado al pawn
esp_df camera
# el nombre se localiza buscando el FString dentro del PlayerState
esp_df names
```

`camera` imprime todos los punteros del PlayerController que son `UObject` de
verdad (vtable dentro del módulo) y busca la POV en cada uno. `names` recorre el
`PlayerState` y valida el `FString` (puntero, longitud y texto UTF-16
imprimible). Con eso se saca el offset nuevo sin adivinar.

Modos de la herramienta:

| Modo | Para qué |
|---|---|
| `probe` | driver, PID, `libUE4.so` y GWorld |
| `test` | ticks con cámara, objetivos, cajas y estadísticas |
| `dump` | un tick con posición/vida/equipo/caja de cada objetivo |
| `chain` | recorre la cadena del jugador y muestra dónde se corta |
| `camera` | busca la POV real dentro del PlayerCameraManager |
| `names` | busca el `FString` del nombre dentro del PlayerState |
| `loop` | bucle continuo para mirar mientras juegas |

## Límites conocidos

- Los offsets son de **esta** build; un parche del juego los invalida
  (`offsets.h` es el único sitio a tocar).
- El FOV es horizontal salvo que se pase `--fov-vertical`; si el juego usa
  una relación de aspecto distinta a la superficie, hay que ajustar `--aspect`.
- La caja usa `BaseEyeHeight + 10 cm` como alto y `0.45 × alto` como ancho
  (`Options::box_width_ratio`): es una aproximación visual, no la cápsula real.
- `skip_teammates` descarta aliados; `--all` los incluye.
- `--min-hp 1` descarta actores sin vida legible (útiles contra el ruido de
  actores que no son personajes).
- Máximo 512 objetivos por frame (límite del contrato `FrameSnapshot`),
  ordenados por distancia.
- El primer tick tras conectar resuelve la ficha de cada actor (RootComponent,
  vida, equipo, nombre). En mapas grandes puede tardar ~1 s; a partir de ahí se
  usa la caché (`cache_ttl_ticks`, 30 ticks por defecto).
- Medido en partida: 177 ticks seguidos a 250 ms sin un solo fallo de lectura,
  9–18 objetivos por tick y cajas de 11–31 px a 31–95 m con pantalla 3088×1440.
- Los nombres solo aparecen cuando el actor tiene `PlayerState`; los bots o
  actores sin estado de jugador salen sin nombre.
- No se ha probado el overlay (ImGui): la validación llega hasta `dc_esp_poll`.
