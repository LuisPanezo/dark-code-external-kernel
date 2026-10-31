#!/usr/bin/env sh
# Diagnostico del lector ESP (Linux/macOS).
#   ./ejecutar.sh [abi] [paquete]
# Todo queda en esp-diagnostico.txt junto a este archivo.
set -eu

HERE=$(cd "$(dirname "$0")" && pwd)
LOG="$HERE/esp-diagnostico.txt"
ABI=${1:-auto}
PKG=${2:-com.garena.game.df}

if [ -x "$HERE/platform-tools/adb" ]; then
    ADB="$HERE/platform-tools/adb"
elif command -v adb >/dev/null 2>&1; then
    ADB=adb
else
    echo "ERROR: no encuentro adb. Copia la carpeta platform-tools junto a este archivo." >&2
    exit 1
fi

"$ADB" start-server >/dev/null 2>&1 || true

if ! "$ADB" devices | grep -q "device$"; then
    echo "No hay dispositivo autorizado."
    echo "  1) activa 'Depuracion USB'"
    echo "  2) conecta el cable y acepta la huella RSA"
    echo "  3) si sigue igual: $ADB kill-server  y repite"
    "$ADB" devices -l
    exit 1
fi

PHONE_ABI=$("$ADB" shell getprop ro.product.cpu.abi | tr -d '\r')
if [ "$ABI" = "auto" ]; then
    case "$PHONE_ABI" in
        arm64-v8a) ABI=arm64 ;;
        x86_64|x86) ABI=x86_64 ;;
        *) echo "ERROR: ABI '$PHONE_ABI' no soportada (se necesita 64 bits)." >&2; exit 1 ;;
    esac
fi

BIN="$HERE/esp_df-$ABI"
[ -f "$BIN" ] || { echo "ERROR: falta esp_df-$ABI junto a este script." >&2; exit 1; }

: > "$LOG"
echo "=== Diagnostico ESP $(date) ===" >> "$LOG"
echo "abi: $ABI ($PHONE_ABI)  paquete: $PKG" >> "$LOG"

step() {
    echo ""
    echo "== $1"
    { echo ""; echo "--- $1 ---"; echo "\$ $2"; } >> "$LOG"
    "$ADB" shell "$2" 2>&1 | tee -a "$LOG"
}

step "modelo y version" "getprop ro.product.model; getprop ro.product.cpu.abi; getprop ro.build.version.release"
step "modulo del kernel" "lsmod | grep phmeop"
step "nodo del driver" "ls -l /dev/phmeop"
step "root" "su -c id"

echo ""
echo "== copiando binario"
"$ADB" push "$BIN" /data/local/tmp/esp_df >/dev/null
"$ADB" shell "su -c 'chmod 755 /data/local/tmp/esp_df'"

step "probe" "su -c '/data/local/tmp/esp_df probe --package $PKG'"
step "test" "su -c '/data/local/tmp/esp_df test --ticks 5 --interval 500 --package $PKG'"
step "dump" "su -c '/data/local/tmp/esp_df dump --raw --package $PKG'"

echo ""
echo "== resultado"
grep -E "GWorld|^CAM:|targets ----|ENC!" "$LOG" | head -6 || true
echo ""
echo "Log completo: $LOG"
echo "Envia ese archivo. Si dice 'GWorld no encontrado', entra a una partida y repite."
