# Dark Code — login Android

Base independiente de interfaz Android nativa, Java 17, Android 8 o posterior.
Diseño crema/negro con fénix, sin el pie «DARK CODE / 01».

## Estado

Vista previa 0.2.0: usuario y contraseña, mostrar/ocultar, validación, conexión HTTPS,
pantalla de cuenta, verificación y cierre de sesión. Backend independiente en `backend/`.
El backend aún no está desplegado y la APK de prueba no tiene URL configurada.
Un formulario válido por sí solo nunca concede acceso. No se guarda la contraseña.
La aplicación no solicita root ni integra un driver.

## Compilación

Requiere JDK 17+ y Android SDK 35. Definir ANDROID_HOME o un local.properties privado.

```sh
./gradlew testDebugUnitTest lintDebug assembleDebug
node --test backend/worker.test.mjs
```

La APK debug es solo para pruebas. Release no tiene firma configurada.
Repositorio privado: https://github.com/LuisPanezo/darkcodeexternalkernel
GitHub Actions comprueba Android y el backend.

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
