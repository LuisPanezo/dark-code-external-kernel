# Dark Code — login Android

Base independiente de interfaz Android nativa, Java 17, Android 8 o posterior.
Diseño crema/negro con fénix, sin el pie «DARK CODE / 01».

## Estado

Vista previa: formulario, validación, mostrar/ocultar PIN, teclado y mensajes accesibles.
No hay servidor de autenticación conectado. Un formulario válido nunca concede acceso.
No se almacena ni se envía el PIN. La aplicación no solicita root ni integra un driver.

## Compilación

Requiere JDK 17+ y Android SDK 35. Definir ANDROID_HOME o un local.properties privado.

```sh
./gradlew testDebugUnitTest lintDebug assembleDebug
```

La APK debug es solo para pruebas. Release no tiene firma configurada.
GitHub Actions prepara la comprobación; necesita publicarse en el repositorio elegido.

## Autenticación pendiente

Un PIN de cuatro dígitos tiene solo 10.000 combinaciones y no constituye por sí solo
un login fuerte. Antes de producción, definir proveedor y política: preferir passkeys
o credenciales robustas, y reservar el PIN para desbloqueo local de una sesión ya validada.
Si el servidor admite PIN, requiere límites de intentos del lado servidor y evaluación
de riesgos; los límites locales no protegen frente a un cliente modificado.

El servidor debe verificar credenciales, emitir sesiones breves y revocables, aplicar
autorización en cada recurso y permitir cierre de sesión. No incluir secretos de servidor
en la APK. Guardar únicamente material de sesión necesario mediante Android Keystore.

Disponibilidad pendiente: tiempos de espera, cancelación, mensajes de desconexión,
reintento explícito y pruebas con servidor caído. Una caída nunca concede acceso.
No se puede proteger una sesión contra todos los ataques en un dispositivo comprometido.

Protecciones de esta base: backup desactivado, tráfico HTTP bloqueado por manifiesto,
PIN no persistido y borrado al pasar a segundo plano; FLAG_SECURE en release.
Estas medidas no sustituyen autenticación ni una revisión de seguridad integral.
