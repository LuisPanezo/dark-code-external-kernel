# Arquitectura de Dark Code 0.3.0

Proyecto Android independiente. Esta versión visualiza una escena local de prueba;
no incluye driver, kernel, acceso a memoria de otras aplicaciones ni offsets.
El login y su backend siguen siendo módulos separados de la visualización.

```text
app/ UI Android (login / cuenta / escena / ventana flotante)
  └─ scene/SceneSurface → bridge JNI → runtime/FrameRuntime
                                         ├─ sources/IFrameSource
                                         │   ├─ MockSource
                                         │   └─ ReplaySource
                                         └─ shared_ptr<const FrameSnapshot>
                                                   ↓
                                      projection → renderer/OpenGL ES

Telemetría de aplicación propia → adapter Unity / Unreal / Custom → FrameSnapshot
```

## Módulos y fronteras

| Carpeta | Responsabilidad | Estado |
|---|---|---|
| `app/.../ui` | Pantalla de escena, navegación y permisos | Implementado |
| `app/.../floating` | Servicio Android, notificación, arrastrar/minimizar/cerrar | Implementado |
| `native/core` | Tipos, cámara, entidades, validez y estados | Implementado |
| `native/adapters` | Normalizar valores de telemetría propia | Implementado y probado con fixtures |
| `native/sources` | Fuente simulada y repetición en memoria | Implementado |
| `native/runtime` | Hilo de adquisición y publicación de snapshots inmutables | Implementado |
| `native/projection` | Matriz de vista/proyección y clipping homogéneo | Implementado |
| `native/renderer` | Geometría y backend OpenGL ES 2 | Implementado |
| `native/bridge` | Propiedad del engine y llamadas JNI | Implementado |
| `backend` | Proxy de autenticación SaoAuth | Escrito, pendiente de despliegue/configuración |

Los adaptadores NO se conectan a juegos ni proyectos Unity/Unreal por sí mismos.
Reciben `TelemetryFrame` con valores exportados explícitamente por una aplicación propia.
El SDK/emisor y el transporte de esa telemetría aún no están implementados.
Los cuatro modos visibles utilizan datos locales de muestra, incluido el modo de repetición.

## Contrato de frame

- `FrameSnapshot`: cámara + vector de entidades + secuencia + timestamp monotónico en ns.
- Cámara: posición, objetivo, vector arriba, FOV vertical en radianes, planos cercano/lejos.
- Entidad: ID no cero y único, centro, semiextensiones positivas, RGBA entre 0 y 1, etiqueta.
- Máximo 512 entidades, etiquetas hasta 128 bytes UTF-8. Números no finitos invalidan el frame.
- Ninguna coordenada específica de motor atraviesa la frontera del adaptador.
- No se representan datos desconocidos con ceros ficticios: la fuente devuelve un estado de error.
- `Normalize` solo reemplaza el resultado cuando el frame completo es válido.

## Convenciones explícitas

Canonical: metros, sistema diestro, +X derecha, +Y arriba, -Z hacia delante.
Matrices por columnas, vectores columna; profundidad OpenGL NDC entre -1 y +1.
La pantalla aplica su propia relación de aspecto en cada cambio de superficie.

| Entrada declarada | Conversión de posición a canonical |
|---|---|
| Propio/canonical | `(x, y, z)` |
| Unity world, metros | `(x, y, -z)` |
| Unreal world, centímetros | `(y / 100, z / 100, -x / 100)` |

La cámara se exporta como posición/objetivo/up para evitar la ambigüedad de matrices
GPU dependientes de plataforma. Las semiextensiones son AABB en ejes del mundo de
entrada; las cajas orientadas y las convenciones personalizadas necesitan otro adaptador.

## Ciclo de vida y rendimiento

`IFrameSource.Start/ReadLatest/Stop` se llama desde un único hilo. `ReadLatest` debe
ser no bloqueante: un futuro transporte deberá imponer sus propios límites y timeout.
El worker intenta adquirir a ~30 Hz; el render usa únicamente el frame más reciente.
No hay cola creciente ni acceso de red en el hilo gráfico. El estado publicado usa mutex
y `shared_ptr<const FrameSnapshot>`, por lo que un consumidor conserva una copia coherente.

Un frame de más de 250 ms se descarta durante reproducción activa. Pausar congela
explícitamente el último frame y muestra `Pausado`, en lugar de presentarlo como vivo.
La pausa de esta primera versión no detiene el reloj de la fuente: al reanudar salta
al tiempo actual. La repetición integrada vuelve al inicio en bucle.

`FrameRuntime` admite inyectar una `SourceFactory` para fuentes propias. Una fuente
que no arranca conserva su estado de error hasta reiniciar/cambiarla; una fuente que
devuelve desconexión durante lectura se consulta de nuevo en el siguiente ciclo.
Excepciones del proveedor se convierten en desconexión, sin conservar un frame engañoso.

Cada superficie tiene su engine, hilo y contexto GL independientes. JNI y recursos GL
se manipulan en el hilo GL. Pausar/minimizar detiene adquisición; cerrar libera engine
y superficie. La aplicación maneja recreación de EGL y cambio de orientación.

## Ventana flotante

Usa el permiso Android `SYSTEM_ALERT_WINDOW`, ventana `TYPE_APPLICATION_OVERLAY` y
servicio en primer plano `specialUse`. No usa root ni pide accesibilidad o captura de pantalla.
Se abre desde la actividad visible; no arranca con el teléfono ni se reinicia sola.
La notificación permite cerrar. La retirada del permiso detiene el servicio.
La ventana captura toques dentro de su rectángulo y no roba el foco del teclado.
Puede arrastrarse desde su título. Al minimizar pasa a un control `DC +` sin render activo.
El tamaño y posición se limitan al espacio de pantalla; el contenido permite desplazamiento.
Publicación en una tienda requiere revisar sus políticas para permisos y foreground services.

## Pruebas reproducibles

```sh
cmake -S native -B native-out -DCMAKE_BUILD_TYPE=Release
cmake --build native-out
ctest --test-dir native-out --output-on-failure
./gradlew testDebugUnitTest lintDebug assembleDebug assembleDebugAndroidTest
node --test backend/worker.test.mjs
```

`PreviewRunner` es una APK de instrumentación separada, no una ruta de autenticación
en producción. Ejecuta pruebas nativas, rechaza login vacío y captura login/cuenta de
demostración/escena/ajustes/estado. En un emulador con permiso de superposición también
ejercita abrir/minimizar/restaurar/cerrar. No otorga permisos por sí misma.
Las capturas de cuenta están marcadas como demostración: no implican login real.

Pendiente fuera de esta entrega: autenticación de producción, emisor de telemetría propia,
importación de archivos de grabación, pruebas en teléfono físico y mediciones sostenidas
de batería/carga. No se promete compatibilidad de todas las variantes de un motor.
