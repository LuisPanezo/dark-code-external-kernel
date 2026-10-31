# Dark Code — Android 0.3.0

Base independiente de interfaz Android nativa y framework de visualización C++17.
Java 17, Android 8 o posterior; ARM64 y x86_64.
Diseño crema/negro con fénix, sin el pie «DARK CODE / 01».

## Estado

Vista previa 0.3.0: usuario y contraseña, mostrar/ocultar, validación, conexión HTTPS,
pantalla de cuenta, verificación y cierre de sesión. Backend independiente en `backend/`.
El backend aún no está desplegado y la APK de prueba no tiene URL configurada.
Un formulario válido por sí solo nunca concede acceso. No se guarda la contraseña.
Tras un login correcto, la app solicita root al gestor de superusuario instalado
mediante `id -u` (timeout de 60 segundos), seguido del permiso Android para
mostrarse sobre otras apps. El acceso root no instala ni integra un driver.
Si root se deniega, la cuenta sigue abierta y permite reintentar; el panel requiere
sesión vigente y consentimiento root. El gestor puede recordar la decisión.

El panel flotante utiliza controles Android nativos, no Dear ImGui. Tiene tema negro,
cinco acentos y selector RGB, opacidad y tamaño de texto, respuesta táctil, perfil
fijo inferior, soporte, arrastre, minimizar/restaurar y cierre desde notificación.
La apariencia se guarda localmente. Los enlaces oficiales de soporte se configuran
en `app/src/main/res/values/support.xml`; vacíos aparecen como «Próximamente».
El perfil utiliza el usuario autenticado. El cliente acepta un objeto opcional
`subscription: {name, expires_at}` con fecha en milisegundos Unix; el backend actual
no lo proporciona, por lo que muestra «sin datos», sin inventar días de suscripción.
La sesión de acceso de 15 minutos es independiente de la suscripción.
La ventana se cierra al salir de la cuenta, invalidar o vencer la sesión.

La escena de prueba OpenGL ES permanece en el código para pruebas de visualización,
pero ya no se ofrece como entrada sin autenticación desde la pantalla de login.

Como módulo aparte, y desactivado hasta que se conecte, está `native/esp`:
lector del juego por `/dev/phmeop`, proyección a pantalla y API C `dc_esp_*`
para un overlay propio (por ejemplo ImGui), más el interruptor desde Java.
No dibuja nada por sí solo ni se activa al arrancar la app. Guía y pasos de
conexión: [native/esp/README.md](native/esp/README.md). El driver vive en
`kernel/` y se compila contra el kernel del dispositivo, no con Gradle.

Contrato, convenciones, límites e integración de fuentes propias: [ARCHITECTURE.md](ARCHITECTURE.md).
Capturas verificadas: [design/README.md](design/README.md).

## Compilación

Requiere JDK 17+, SDK 35, NDK 28.2.13676358 y CMake 3.22.1.
Definir ANDROID_HOME o un local.properties privado.

```sh
./gradlew testDebugUnitTest lintDebug assembleDebug
node --test backend/worker.test.mjs
```

La APK debug es solo para pruebas. Release no tiene firma configurada.
Repositorio privado: https://github.com/LuisPanezo/darkcodeexternalkernel
GitHub Actions comprueba Android y el backend.

Pruebas realizadas en emulador Android 15 x86_64: 19 comprobaciones nativas, cambio
entre cuatro fuentes, pausa, orientación y abrir/minimizar/restaurar/cerrar la ventana.
También pasan 4 pruebas Java y 9 del backend. Lint sin errores, con advertencias
de localización y compatibilidad que aún requieren revisión. Falta teléfono físico.

## Autenticación pendiente

SaoAuth se conecta desde el backend, con secretos independientes de otros proyectos.
La configuración, límites y pasos pendientes están en `backend/README.md`.
La contraseña no se recorta ni se transforma; las políticas para crear contraseñas
pertenecen al proveedor, no a la validación del formulario de acceso.
Sesiones opacas de 15 minutos en memoria de la APK. No hay acceso offline ni renovación
automática. La aplicación verifica de nuevo la sesión al volver del segundo plano.
La API incorpora límites persistentes, revocación, timeout y respuestas sin caché.
Falta desplegar con el secreto nuevo y probar con una cuenta real. Las pruebas locales
simulan SaoAuth; no demuestran que el proveedor acepte esta configuración nueva.
No se puede proteger una sesión contra todos los ataques en un dispositivo comprometido.

Protecciones de esta base: backup desactivado, tráfico HTTP bloqueado por manifiesto,
contraseña no persistida y borrada al pasar a segundo plano; FLAG_SECURE en release.
Estas medidas no sustituyen autenticación ni una revisión de seguridad integral.
