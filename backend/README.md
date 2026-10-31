# Backend independiente de autenticación

Este servicio solo gestiona login, verificación y cierre de sesión para la nueva APK.
No reutiliza secretos, sesiones, bases de datos, archivos ni despliegues de otras apps.
La operación de login usa `type=login`, verificada contra la aplicación nueva.
Login, consulta de sesión, logout y rechazo del token revocado verificados en vivo
con una cuenta de prueba de un día. La respuesta actual es `success` + `info`, sin token del proveedor.
No hay credenciales reales en este repositorio.

## Estado del despliegue (2026-10-30)

Worker publicado: `https://darkcode-login-auth.darkcode-api.workers.dev`.
`GET /health` verificado: `{"status":"ok"}`. Durable Object `AUTH_STATE` desplegado.
La URL está configurada en `gradle.properties` para las próximas compilaciones.
`SAO_OWNERID` y `SAO_SECRET` guardados como secretos y verificados por nombre.
Corregida la compatibilidad con Workers: redirecciones manuales y rechazo de respuestas 3xx.
APK de interfaz compilada e instalada con esta URL. Login real verificado por API;
queda pendiente comprobar el flujo completo en el teléfono.
Corregido el rechazo `unsupported_action`: el proveedor reconoce `type=login`.

## Pasos pendientes

1. Regenerar el secreto que apareció en la captura, desde SaoAuth.
2. En la cuenta de Cloudflare elegida, configurar `SAO_OWNERID` y `SAO_SECRET`
   mediante entradas interactivas de Wrangler, sin pasarlos por argumentos del shell.
3. Verificar las variables del Worker independiente `darkcode-login-auth`.
4. Compilar la APK; `authBaseUrl` ya apunta al Worker (puede sobrescribirse con `-PauthBaseUrl`).
5. Probar un usuario válido de la aplicación nueva, rechazo, revocación y caída del proveedor.

```sh
node --test backend/worker.test.mjs
npx wrangler secret put SAO_OWNERID --config backend/wrangler.jsonc
npx wrangler secret put SAO_SECRET --config backend/wrangler.jsonc
npx wrangler deploy --config backend/wrangler.jsonc
```

No enviar contraseñas reales a un despliegue sin terminar de revisar su configuración.
La configuración se llama `darkcode-login-auth`; el dominio final depende de la cuenta.

## Contrato propio

- `POST /v1/login`: JSON `username`, `password`, `device_id` (UUID por instalación).
  Devuelve `session_token` opaco, `username`, `expires_at` en milisegundos Unix.
- `POST /v1/session`: `Authorization: Bearer TOKEN` y JSON `device_id`.
  Valida en el gateway en cada consulta. Si existe token del proveedor, también lo revalida.
  El formato actual sin token usa una sesión local de cinco minutos; no consulta revocaciones
  de SaoAuth durante ese intervalo. Un nuevo login vuelve a comprobar el acceso.
- `POST /v1/logout`: mismo encabezado/cuerpo. Revoca primero nuestra sesión;
  `upstream_revoked: false` indica que SaoAuth no confirmó su revocación.
- `GET /health`: comprueba que el Worker responde, NO la disponibilidad de SaoAuth.

Sesiones de cinco minutos para el formato actual sin token, limitadas además por la
fecha de vencimiento numérica que devuelve SaoAuth. No admiten renovación automática.
El formato con token conserva sesiones de 15 minutos. El token del proveedor permanece
en almacenamiento privado del Durable Object; la APK recibe otro token aleatorio.
La APK mantiene la sesión solo en memoria: cerrar su proceso requiere entrar de nuevo.
El UUID asocia la sesión a la instalación, pero puede copiarse; no es attestation.

Contadores persistentes y transaccionales: 5 intentos de login por cuenta/minuto,
20 por IP/minuto; las consultas de sesión permiten hasta 120 por IP/minuto.
Los contadores por IP son compartidos entre rutas. Estos límites pueden afectar
a usuarios que comparten IP o sufrir bloqueo temporal inducido; ajustar tras observar uso.
No hay reintentos automáticos de contraseñas, logs de cuerpos ni mensajes crudos del proveedor.
Las respuestas son `no-store`, con tamaño limitado y llamadas salientes con timeout.

No se implementa autorización por licencia ni acceso a módulos; una sesión autenticada
no constituye autorización para operaciones futuras. Cada recurso deberá verificar permisos.
No se ha realizado auditoría integral ni prueba de carga/alta disponibilidad.
Las pruebas de unidad usan un almacenamiento simulado: requieren validación adicional
en el runtime de Cloudflare y en un teléfono antes de producción.
