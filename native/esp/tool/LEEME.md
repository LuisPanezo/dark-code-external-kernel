# Diagnóstico ESP — instrucciones

Esta carpeta sirve para **una sola cosa**: ejecutar un lector de prueba contra el
juego y generar un archivo de registro (`esp-diagnostico.txt`) para saber si los
datos del juego se leen bien. No instala nada, no modifica el juego y no dibuja
nada en pantalla: solo lee memoria y escribe un log.

## Lo que necesitas

- Teléfono Android **de 64 bits** (cualquier móvil de los últimos años).
- **Root** funcionando (Magisk u similar) y que `su` conceda permisos.
- El **driver `/dev/phmeop` ya cargado** en el teléfono.
- Un cable USB y una PC con Windows.

> Si `ls -l /dev/phmeop` no existe, el driver no está cargado: **para y avisa**.
> El driver se compila para el kernel exacto de cada teléfono; no se puede copiar
> de un móvil a otro.

## Pasos

1. Descomprime esta carpeta donde quieras (por ejemplo el Escritorio).
2. En el teléfono: Ajustes → Opciones de desarrollador → activa **Depuración USB**.
3. Conecta el cable y, cuando el teléfono pregunte, acepta la **huella RSA**.
4. En la PC, abre PowerShell en esta carpeta y ejecuta:

   ```powershell
   powershell -ExecutionPolicy Bypass -File .\ejecutar.ps1
   ```

   En Linux o macOS:

   ```sh
   chmod +x ejecutar.sh && ./ejecutar.sh
   ```

5. Entra a **una partida** (no al menú principal) y vuelve a ejecutar el comando
   si el primero dijo «GWorld no encontrado».
6. Cuando Magisk pida permiso de root, **concede**.
7. Al terminar, envía el archivo **`esp-diagnostico.txt`** que aparece en esta
   carpeta.

## Qué significa la salida

| Línea | Significado |
|---|---|
| `[+] PID juego: 12345` | encontró el proceso del juego |
| `[+] GWorld @ 0x… , world 0x…` | encontró el estado de la partida |
| `CAM: loc=… rot=… fov=90` | la cámara se lee bien |
| `---- 7 targets ----` | detectó 7 objetivos |
| `BOX x=… y=… w=… h=…` | ✅ todo correcto |
| `[ENC!]` | las posiciones vienen cifradas: hay que mandar el dump |
| `[-] EBUSY` | otra app tiene el driver abierto: ciérrala y repite |
| `[-] EPERM` | no se concedió root |
| `[-] GWorld no encontrado` | estás en el menú principal o cargando |

## Problemas típicos

- **`No hay dispositivo autorizado`**: falta activar Depuración USB o aceptar la
  huella RSA. Prueba `.\platform-tools\adb.exe devices`.
- **`adb: no devices/emulators found`**: cable de datos (no solo carga), otro
  puerto USB o falta el driver USB del fabricante.
- **`Permission denied` al ejecutar**: se olvidó el `chmod`; el script ya lo hace.
  Si falla, ejecuta a mano
  `.\platform-tools\adb.exe shell "su -c 'chmod 755 /data/local/tmp/esp_df'"`.
- **`su: not found`**: el teléfono no tiene root o Magisk lo denegó.

## Importante

- Úsalo **solo en tu propio teléfono**. No modifica el juego, pero lee memoria de
  otro proceso y eso requiere root.
- No hace falta instalar la APK ni el juego debe estar abierto en primer plano
  para leer la partida, pero **la partida sí tiene que existir** (estar dentro).
- Cierra la app que use `/dev/phmeop` antes de probar: el driver atiende a un
  solo cliente a la vez.
