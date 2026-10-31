# Integración del ESP en otro proyecto

Paquete autocontenido: fuentes, CMake y puente Java. No necesitas este
repositorio para compilar ni para ejecutar.

## Qué hay dentro

```text
esp-integracion/
├── CMakeLists.txt          targets esp_core (lectura + proyección) y esp_jni
├── kernel/phmeop.h         contrato del driver (lo incluye esp/abi.h)
├── native/
│   ├── esp/                módulo ESP (driver, escáner, lector, proyector, API)
│   ├── core/               Vec3, cámara, FrameSnapshot, Status
│   ├── adapters/           conversión UE → canónico (la usa el lector)
│   └── projection/         vista/proyección (la usa el proyector)
└── java/com/darkcode/login/esp/EspNative.java
```

`native/esp/examples/overlay_imgui.cpp` es el overlay de ejemplo listo para
pegar (caja, vida, nombre, distancia, snapline y panel con el interruptor).

## Añadirlo al proyecto

```cmake
add_subdirectory(ruta/a/esp-integracion)
target_link_libraries(mi_overlay PRIVATE esp_core)   # API C
# solo si enciendes el ESP desde Java:
target_link_libraries(mi_overlay PRIVATE esp_jni)
```

Los dos targets ya exportan los `include_directories`, así que basta con:

```cpp
#include "esp/api/esp_api.h"
```

## Uso

```cpp
// 1) una vez (root + driver cargado)
DcEspConfig config{};
config.package        = "com.garena.game.df";
config.pid            = 0;     // 0 = buscar por paquete
config.min_health     = 1;     // descarta actores sin vida legible
config.skip_teammates = 1;
config.max_distance_m = 250;
config.poll_interval_ms = 16;
dc_esp_attach_config(&config);

// 2) cada frame, en el hilo de dibujo (cero ioctl)
DcEspBox boxes[64];
const int count = dc_esp_poll(ancho, alto, boxes, 64);

// 3) interruptor
dc_esp_set_enabled(1);
```

`dc_esp_attach*` **no bloquea**: arranca un hilo que lee y publica; el escaneo
de GWorld tarda unos cientos de ms y `dc_esp_status()` informa del progreso.
Si el juego no está abierto, reintenta cada 2 s.

Campos de `DcEspBox`: `x, y, w, h` (píxeles, origen arriba-izquierda),
`distance_m`, `health`, `team`, `name`, `flags`
(`DC_ESP_FLAG_ENCRYPTED`, `DC_ESP_FLAG_TEAMMATE`, `DC_ESP_FLAG_DEAD`,
`DC_ESP_FLAG_NO_HEALTH`).

## Requisitos

- Android de **64 bits** (arm64-v8a). El ABI del driver es LP64.
- **Root** y `/dev/phmeop` cargado (`insmod phmeop.ko`, compilado para el
  kernel exacto del dispositivo).
- El viewport que pases a `dc_esp_poll` debe ser **el del juego**, no el de la
  pantalla, si hay letterbox o recorte por notch.

## Aviso importante (pendiente)

La app Android **no es root**: `RootAccess` solo ejecuta `id -u`. Abrir
`/dev/phmeop` desde el proceso de la app devuelve `EPERM`. Para usarlo desde la
APK hace falta el helper `espd` (proceso root + socket), que aún no está
implementado. Mientras tanto, el único camino verificado es la herramienta
`esp_df` lanzada con `su`.

## Puente Java

`EspNative.java` está en el paquete `com.darkcode.login.esp` y los símbolos JNI
de `esp_jni.cpp` se llaman igual. Si cambias el paquete, renombra también las
funciones (`Java_<paquete_con_guiones_bajos>_EspNative_<metodo>`) o la JVM no
encontrará los métodos y lanzará `UnsatisfiedLinkError`.

## Si prefieres copiar archivos a mano

Fuentes del módulo (`native/esp/`):

```text
abi.h  offsets.h  driver.h  driver_linux.cpp  process_linux.cpp
elf.h  elf.cpp  types.h  types.cpp
scanner.h  scanner.cpp  ue.h  ue.cpp  reader.h  reader.cpp
projector.h  projector.cpp
api/esp_api.h  api/esp_api.cpp  api/esp_service.h  api/esp_service.cpp
esp_jni.cpp  examples/overlay_imgui.cpp
```

Dependencias del motor (`native/`):

```text
core/types.h  core/coordinate_system.h  core/status.h
core/frame_snapshot.h  core/frame_snapshot.cpp
adapters/normalize.h  adapters/normalize.cpp
projection/projection.h  projection/projection.cpp
```

Y `kernel/phmeop.h`, que `esp/abi.h` incluye por ruta relativa
(`../../kernel/phmeop.h`): mantén esa profundidad de carpetas o ajusta el
`#include`.
