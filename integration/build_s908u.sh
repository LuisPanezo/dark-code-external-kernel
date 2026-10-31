#!/usr/bin/env bash
#
# build_s908u.sh — compila el driver phmeop (con fix de THP) para el
# Galaxy S22 Ultra (SM-S908U), contra el fuente exacto Samsung android12-5.10.
#
# Ya trae incorporado el arreglo de THP (sub-page en compound pages) en
# kernel/entry.c. Este script solo lo convierte en el .ko listo para insmod.
#
# Uso:
#   export KDIR=/ruta/al/kernel-samsung-5.10.209
#   ./build_s908u.sh
#
# Opciones (todas con valores por defecto razonables):
#   KDIR           ruta al fuente Samsung 5.10.209 CONFIGURADO (con .config)
#   CROSS_COMPILE  prefijo del toolchain (def. aarch64-linux-android-)
#   ARCH           arquitectura (def. arm64)
#
# Requisitos previos en la máquina Linux:
#   1) Fuente Samsung del SM-S908U configurado con el .config del dispositivo
#      (se saca de /proc/config.gz del teléfono) y con "make modules_prepare"
#      ya ejecutado una vez.
#   2) Toolchain aarch64 (gcc) en el PATH.
set -euo pipefail

cd "$(dirname "$0")/.."

ARCH="${ARCH:-arm64}"
CROSS_COMPILE="${CROSS_COMPILE:-aarch64-linux-android-}"
KDIR="${KDIR:-}"

if [[ -z "$KDIR" ]]; then
    echo "[-] Define KDIR con la ruta al fuente del kernel Samsung." >&2
    echo "    Ejemplo: KDIR=/ruta/al/kernel-samsung-5.10.209 $0" >&2
    exit 1
fi

if [[ ! -f "$KDIR/.config" ]]; then
    echo "[-] No encuentro $KDIR/.config" >&2
    echo "    El fuente debe estar CONFIGURADO (make modules_prepare)." >&2
    exit 1
fi

echo "[*] Compilando phmeop.ko"
echo "    KDIR=$KDIR"
echo "    ARCH=$ARCH  CROSS_COMPILE=$CROSS_COMPILE"
echo ""

make -C kernel ARCH="$ARCH" CROSS_COMPILE="$CROSS_COMPILE" KDIR="$KDIR" -j"$(nproc)"

KO="kernel/phmeop.ko"
if [[ -f "$KO" ]]; then
    echo ""
    echo "[+] $KO generado."
    echo "    vermagic: $(modinfo "$KO" 2>/dev/null | awk '/vermagic/{print $2}' || strings "$KO" | grep -m1 'vermagic=')"
    echo ""
    echo "    Para instalarlo en el teléfono:"
    echo "      adb push $KO /data/local/tmp/phmeop.ko"
    echo "      adb shell \"su -c 'insmod /data/local/tmp/phmeop.ko'\""
    echo "      adb shell \"su -c 'ls -l /dev/phmeop'\""
else
    echo "[-] No se generó $KO" >&2
    exit 1
fi
