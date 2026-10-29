# Validación de la versión 0.3.0

29 de octubre de 2026 — compilación Windows, ejecución en emulador Android 15 x86_64.

| Comprobación | Resultado |
|---|---|
| APK debug, ARM64 + x86_64 | Compila |
| APK release con R8, ARM64 + x86_64 | Compila; no firmada para distribución |
| Pruebas Java del login | 4 aprobadas |
| Pruebas del backend con proveedor simulado | 9 aprobadas |
| Contratos/proyección/fuentes/runtime C++ en Android | 19 aprobadas |
| Login vacío | Rechazado |
| Propio/Unity/Unreal/repetición simulados | Renderizan 3 objetos |
| Pausa y reanudación | Aprobado |
| Rotación vertical/horizontal | Aprobado; capturas guardadas |
| Ventana flotante: abrir/minimizar/restaurar/cerrar | Aprobado |
| Lint debug | 0 errores, 22 advertencias |

El permiso de superposición se habilitó temporalmente en el emulador para la prueba
y se restauró a su estado `default`. No se cambiaron permisos en un teléfono físico.
El runner de captura se compila en una APK de pruebas separada y no permite autenticar usuarios.

Las advertencias de Lint incluyen textos sin recursos de traducción, icono de launcher,
constructor de vista programática, accesibilidad del arrastre y coordenadas físicas LEFT
de la ventana. El arrastre conserva `performClick()` para tocar/restaurar.
No se ha hecho auditoría completa de accesibilidad ni de localización.

No se probó: hardware ARM64 físico, un emisor Unity/Unreal real, backend desplegado,
autenticación real de SaoAuth, autonomía/batería prolongada ni publicación en tienda.
La inspección visual de capturas no sustituye pruebas exhaustivas en todos los tamaños.

Capturas vigentes en `design/current/`. APK debug de entrega en
`dist/DarkCode-0.3.0-scene-preview.apk` (ignorada por Git; CI publica su propia APK como artifact).
