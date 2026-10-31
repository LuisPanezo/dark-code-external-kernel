#!/usr/bin/env sh
# Compila esp_df (herramienta de diagnóstico) para Android con el NDK.
#
#   NDK=~/android-ndk-r28 ./build_android.sh            # arm64-v8a, API 29
#   NDK=~/android-ndk-r28 ABI=x86_64 ./build_android.sh # emulador
#
# El driver .ko no se compila aquí (depende del kernel del dispositivo):
# ver ../../../kernel/README.md.
set -eu

ROOT=$(cd "$(dirname "$0")/../../.." && pwd)
NDK=${NDK:-${ANDROID_NDK_HOME:-}}
if [ -z "$NDK" ]; then
    echo "define NDK (ruta del Android NDK) o ANDROID_NDK_HOME" >&2
    exit 1
fi

ABI=${ABI:-arm64-v8a}
API=${API:-29}
BUILD=${BUILD:-$ROOT/build-esp-$ABI}
CMAKE=${CMAKE:-cmake}

[ -f "$NDK/build/cmake/android.toolchain.cmake" ] || {
    echo "no encuentro $NDK/build/cmake/android.toolchain.cmake" >&2
    exit 1
}

"$CMAKE" -S "$ROOT/native" -B "$BUILD" \
    -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI="$ABI" \
    -DANDROID_PLATFORM="android-$API" \
    -DCMAKE_BUILD_TYPE=Release \
    -DDARKCODE_ESP_TOOL=ON

"$CMAKE" --build "$BUILD" --target esp_df --parallel

echo
echo "binario: $BUILD/esp_df"
echo "instalar y probar:"
echo "  adb push \"$BUILD/esp_df\" /data/local/tmp/"
echo "  adb shell \"su -c chmod 755 /data/local/tmp/esp_df\""
echo "  adb shell \"su -c /data/local/tmp/esp_df probe\""
