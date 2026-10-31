# package_test.ps1 — arma una carpeta lista para enviar a otra persona.
#
#   .\package_test.ps1                 compila arm64 + x86_64 y crea dist\esp-test
#   .\package_test.ps1 -SkipBuild      reutiliza build-esp-* si ya existen
#   .\package_test.ps1 -NoAdb          no copia platform-tools (el otro PC ya tiene adb)
#
# Genera:
#   dist\esp-test\            (carpeta para copiar/enviar)
#   dist\esp-test.zip         (mismo contenido comprimido)
#
# Contenido: esp_df-arm64, esp_df-x86_64, ejecutar.ps1, ejecutar.sh, LEEME.md,
# MANIFIESTO.txt y (opcional) platform-tools.

param(
    [switch]$SkipBuild,
    [switch]$NoAdb
)

$ErrorActionPreference = "Stop"
$root = (Resolve-Path (Join-Path $PSScriptRoot "..\..\..")).Path
$native = Join-Path $root "native"
$stage = Join-Path $root "dist\esp-test"
$zip = Join-Path $root "dist\esp-test.zip"

# ── herramientas ──
$sdk = $env:ANDROID_HOME
if (-not $sdk) { $sdk = $env:ANDROID_SDK_ROOT }
if (-not $sdk) { $sdk = Join-Path $env:LOCALAPPDATA "Android\Sdk" }
if (-not (Test-Path $sdk)) { throw "no encuentro el SDK de Android (define ANDROID_HOME)." }

$cmake = $null
$candidate = Join-Path $sdk "cmake\3.22.1\bin\cmake.exe"
if (Test-Path $candidate) { $cmake = $candidate }
if (-not $cmake) {
    $found = Get-Command cmake -ErrorAction SilentlyContinue
    if ($found) { $cmake = $found.Source }
}
if (-not $cmake) { throw "no encuentro cmake (ni en el SDK ni en PATH)." }

$ninja = $null
$ninjaPath = Join-Path (Split-Path $cmake) "ninja.exe"
if (Test-Path $ninjaPath) { $ninja = $ninjaPath }
else {
    $found = Get-Command ninja -ErrorAction SilentlyContinue
    if ($found) { $ninja = $found.Source }
}

$ndk = $env:NDK
if (-not $ndk) { $ndk = $env:ANDROID_NDK_HOME }
if (-not $ndk) {
    $ndkRoot = Join-Path $sdk "ndk"
    if (Test-Path $ndkRoot) {
        $ndk = (Get-ChildItem $ndkRoot -Directory | Sort-Object Name | Select-Object -Last 1).FullName
    }
}
if (-not $ndk -or -not (Test-Path (Join-Path $ndk "build\cmake\android.toolchain.cmake"))) {
    throw "no encuentro el NDK (define NDK o instala uno en el SDK)."
}

$toolchain = Join-Path $ndk "build\cmake\android.toolchain.cmake"
Write-Host "SDK:   $sdk"
Write-Host "NDK:   $ndk"
Write-Host "CMake: $cmake"

# ── compilar ──
$abis = @(
    @{ Name = "arm64-v8a"; Suffix = "arm64" },
    @{ Name = "x86_64";    Suffix = "x86_64" }
)

if (-not $SkipBuild) {
    foreach ($abi in $abis) {
        $build = Join-Path $root "build-esp-$($abi.Suffix)"
        Write-Host "`n== compilando $($abi.Name)"
        $arguments = @("-S", $native, "-B", $build,
            "-DCMAKE_TOOLCHAIN_FILE=$toolchain",
            "-DANDROID_ABI=$($abi.Name)",
            "-DANDROID_PLATFORM=android-29",
            "-DCMAKE_BUILD_TYPE=Release",
            "-DDARKCODE_ESP_TOOL=ON")
        if ($ninja) {
            $arguments += @("-G", "Ninja", "-DCMAKE_MAKE_PROGRAM=$ninja")
        }
        & $cmake @arguments | Out-Null
        if ($LASTEXITCODE -ne 0) { throw "cmake configure falló para $($abi.Name)" }
        & $cmake --build $build --target esp_df | Out-Null
        if ($LASTEXITCODE -ne 0) { throw "compilación falló para $($abi.Name)" }
    }
}

# ── armar carpeta ──
if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
New-Item -ItemType Directory -Force -Path $stage | Out-Null

$files = @()
foreach ($abi in $abis) {
    $built = Join-Path $root "build-esp-$($abi.Suffix)\esp_df"
    if (-not (Test-Path $built)) { throw "falta $built (compila sin -SkipBuild)." }
    $destination = Join-Path $stage "esp_df-$($abi.Suffix)"
    Copy-Item $built $destination -Force
    $files += $destination
}

foreach ($name in @("ejecutar.ps1", "ejecutar.sh", "LEEME.md")) {
    $source = Join-Path $PSScriptRoot $name
    if (-not (Test-Path $source)) { throw "falta $source" }
    Copy-Item $source (Join-Path $stage $name) -Force
}

$adbIncluded = $false
if (-not $NoAdb) {
    $candidates = @(
        "C:\Users\$env:USERNAME\Documents\adb\platform-tools",
        "$sdk\platform-tools"
    )
    foreach ($candidate in $candidates) {
        if (Test-Path (Join-Path $candidate "adb.exe")) {
            Copy-Item $candidate (Join-Path $stage "platform-tools") -Recurse -Force
            $adbIncluded = $true
            break
        }
    }
    if (-not $adbIncluded) { Write-Host "aviso: no encontré platform-tools; se omite adb." -ForegroundColor Yellow }
}

# ── manifiesto con hashes ──
$lines = @()
$lines += "ESP diagnostico - paquete de prueba"
$lines += "generado: $(Get-Date -Format s)"
$lines += "NDK: $ndk"
$lines += "ABIs: arm64-v8a, x86_64 (Android 10+, solo 64 bits)"
$lines += "adb incluido: $(if ($adbIncluded) { 'si' } else { 'no' })"
$lines += ""
$lines += "archivos:"
foreach ($file in (Get-ChildItem $stage -File)) {
    $hash = (Get-FileHash -Path $file.FullName -Algorithm SHA256).Hash
    $lines += ("  {0,-18} {1,8} bytes  {2}" -f $file.Name, $file.Length, $hash)
}
$lines += ""
$lines += "uso: powershell -ExecutionPolicy Bypass -File .\ejecutar.ps1"
$lines += "enviar de vuelta: esp-diagnostico.txt"
Set-Content -Path (Join-Path $stage "MANIFIESTO.txt") -Value $lines -Encoding UTF8

# ── comprimir ──
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path $stage -DestinationPath $zip -Force

$sizeMb = [math]::Round((Get-Item $zip).Length / 1MB, 1)
Write-Host "`n== listo" -ForegroundColor Green
Get-ChildItem $stage | ForEach-Object {
    $kind = if ($_.PSIsContainer) { "<dir>" } else { "$([math]::Round($_.Length/1KB)) KB" }
    Write-Host ("   {0,-18} {1}" -f $_.Name, $kind)
}
Write-Host "`ncarpeta: $stage"
Write-Host "zip:     $zip ($sizeMb MB)"
