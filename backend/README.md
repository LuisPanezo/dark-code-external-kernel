# Backend independiente de autenticación

Este servicio solo gestiona login, verificación y cierre de sesión para la nueva APK.
No reutiliza secretos, sesiones, bases de datos, archivos ni despliegues de otras apps.
El contrato SaoAuth se tomó como referencia del código local anterior; falta verificarlo
en vivo contra la aplicación nueva. No hay credenciales reales en este repositorio.

## Despliegue pendiente

1. Regenerar el secreto que apareció en la captura, desde SaoAuth.
2. En la cuenta de Cloudflare elegida, configurar `SAO_OWNERID` y `SAO_SECRET`
   mediante entradas interactivas de Wrangler, sin pasarlos por argumentos del shell.
3. Desplegar este Worker con su propio Durable Object. No usar la configuración del otro proyecto.
4. Compilar la APK con `-PauthBaseUrl=https://DOMINIO-DEL-WORKER` (sin ruta ni barra final).
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
  Revalida en SaoAuth en cada consulta. No acepta acceso sin red.
- `POST /v1/logout`: mismo encabezado/cuerpo. Revoca primero nuestra sesión;
  `upstream_revoked: false` indica que SaoAuth no confirmó su revocación.
- `GET /health`: comprueba que el Worker responde, NO la disponibilidad de SaoAuth.

Sesiones de 15 minutos sin renovación automática. El token del proveedor permanece
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
