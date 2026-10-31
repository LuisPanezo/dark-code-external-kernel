# export_sources.ps1 - arma el paquete para integrar el ESP en OTRO proyecto.
#
#   .\export_sources.ps1
#
# Genera dist\esp-integracion\ con las fuentes necesarias, CMake y el puente
# Java, y dist\esp-integracion.zip. No incluye tests, herramienta ni binarios.

param(
    [switch]$NoZip
)

$ErrorActionPreference = "Stop"
$root = (Resolve-Path (Join-Path $PSScriptRoot "..\..\..")).Path
$source = Join-Path $root "native"
$output = Join-Path $root "dist\esp-integracion"
$zip = Join-Path $root "dist\esp-integracion.zip"

if (Test-Path $output) { Remove-Item -Recurse -Force $output }

function Copy-Relative {
    param([string]$From, [string]$To)
    $target = Join-Path $output $To
    New-Item -ItemType Directory -Force -Path (Split-Path $target) | Out-Null
    Copy-Item (Join-Path $root $From) $target -Force
}

# --- modulo ESP ---
$espFiles = @(
    "esp\abi.h", "esp\offsets.h", "esp\types.h", "esp\types.cpp",
    "esp\driver.h", "esp\driver_linux.cpp", "esp\process_linux.cpp",
    "esp\elf.h", "esp\elf.cpp", "esp\scanner.h", "esp\scanner.cpp",
    "esp\ue.h", "esp\ue.cpp", "esp\reader.h", "esp\reader.cpp",
    "esp\projector.h", "esp\projector.cpp",
    "esp\api\esp_api.h", "esp\api\esp_api.cpp",
    "esp\api\esp_service.h", "esp\api\esp_service.cpp",
    "esp\esp_jni.cpp",
    "esp\examples\overlay_imgui.cpp",
    "esp\README.md"
)
foreach ($file in $espFiles) { Copy-Relative "native\$file" "native\$file" }

# --- dependencias del motor ---
$engineFiles = @(
    "core\types.h", "core\coordinate_system.h", "core\status.h",
    "core\frame_snapshot.h", "core\frame_snapshot.cpp",
    "adapters\normalize.h", "adapters\normalize.cpp",
    "projection\projection.h", "projection\projection.cpp",
    "sources\frame_source.h"
)
foreach ($file in $engineFiles) { Copy-Relative "native\$file" "native\$file" }

# --- contrato del driver ---
Copy-Relative "kernel\phmeop.h" "kernel\phmeop.h"

# --- puente Java ---
Copy-Relative "app\src\main\java\com\darkcode\login\esp\EspNative.java" `
              "java\com\darkcode\login\esp\EspNative.java"

# --- guia ---
Copy-Relative "native\esp\examples\INTEGRACION.md" "LEEME.md"

# --- CMake del paquete ---
$cmake = @'
cmake_minimum_required(VERSION 3.22)
project(darkcode_esp LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(esp_core STATIC
    native/core/frame_snapshot.cpp
    native/adapters/normalize.cpp
    native/projection/projection.cpp
    native/esp/types.cpp
    native/esp/driver_linux.cpp
    native/esp/process_linux.cpp
    native/esp/elf.cpp
    native/esp/scanner.cpp
    native/esp/ue.cpp
    native/esp/reader.cpp
    native/esp/projector.cpp
    native/esp/api/esp_service.cpp
    native/esp/api/esp_api.cpp)
target_include_directories(esp_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/native
    ${CMAKE_CURRENT_SOURCE_DIR}/native/esp)
set_target_properties(esp_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(NOT MSVC)
    target_compile_options(esp_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Puente JNI opcional: enlazalo solo si enciendes el ESP desde Java.
add_library(esp_jni STATIC native/esp/esp_jni.cpp)
target_link_libraries(esp_jni PUBLIC esp_core)
set_target_properties(esp_jni PROPERTIES POSITION_INDEPENDENT_CODE ON)
'@
Set-Content -Path (Join-Path $output "CMakeLists.txt") -Value $cmake -Encoding ASCII

# --- resumen ---
$files = Get-ChildItem $output -Recurse -File
Write-Host ("archivos: {0}  ({1} KB)" -f $files.Count,
            [math]::Round(($files | Measure-Object Length -Sum).Sum / 1KB))

if (-not $NoZip) {
    if (Test-Path $zip) { Remove-Item $zip -Force }
    Compress-Archive -Path $output -DestinationPath $zip -Force
    Write-Host ("zip: {0} ({1} KB)" -f $zip,
                [math]::Round((Get-Item $zip).Length / 1KB))
}
Write-Host "carpeta: $output"
