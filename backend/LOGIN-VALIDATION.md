# Verificación del login — 2026-10-31

## Comprobado en esta revisión

- 14 pruebas del backend aprobadas: aceptación/rechazo del proveedor, formato de
  login, sesiones ligadas al dispositivo, vencimiento, revocación, redirecciones,
  límite de intentos, respuestas malformadas y errores de configuración.
- 7 pruebas Java aprobadas (LoginPolicy y AccountState).
- `testDebugUnitTest lintDebug assembleDebug -PpanelOnly=true`: correcto.
  Lint conserva advertencias; no es una auditoría completa.
- Worker desplegado: `cf60f0da-af40-4310-81af-0ee1f08ceef3`.
- La cuenta de prueba existente recibe `hwid_mismatch` al intentar acceder
  desde el identificador de pruebas anterior. No se restableció su vínculo.
- APK de interfaz preparada sin el ejecutable `assets/esp_df`, con el servicio
  adicional deshabilitado y sin enlazar el módulo nativo ESP.

## Correcciones

- El cliente usa `success` del gateway para confirmar el cierre, en lugar de
  confundir la ausencia de token SaoAuth con un fallo de logout.
- Al vencer la sesión, se invalidan callbacks pendientes y permisos en curso.
- Mensajes distintos para dispositivo vinculado, cuenta vencida, configuración,
  límite de intentos, formato incorrecto y tiempo de espera.
- Fecha de suscripción validada y enviada por el backend; es independiente del
  vencimiento de cinco minutos de la sesión. No se inventa el nombre del plan.
- Se rechazan fechas numéricas fuera del rango de enteros seguros.

## Pendiente para declarar el flujo completo verificado

APK instalada en Samsung SM-S908U. La prueba de instrumentación del cliente
Android real aprobó: rechazo de contraseña incorrecta, login válido, fecha de
suscripción futura, consulta de sesión, logout y rechazo del token revocado.
El resultado está en `android-auth-validation.txt`. Se eliminó el archivo privado
de credenciales temporales y se desinstaló el APK auxiliar de pruebas.

Falta comprobar mediante la interfaz la pérdida de red, volver del segundo plano,
vencimiento y paso a los permisos root y de superposición. La prueba automatizada
no pulsa controles ni solicita root.

El login real, consulta y logout también fueron comprobados por API. La sesión actual sin token
de proveedor dura hasta cinco minutos; cambios de acceso en SaoAuth se comprueban
en el siguiente login, no de manera inmediata dentro de ese intervalo.
