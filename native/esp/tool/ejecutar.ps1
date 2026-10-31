param(
    [string]$Abi = "auto",
    [string]$Serial = "",
    [int]$Ticks = 5,
    [string]$Package = "com.garena.game.df"
)

# Diagnostico del lector ESP.
# Copia esp_df al telefono y ejecuta probe/test/dump.
# Todo queda en esp-diagnostico.txt (junto a este archivo): eso es lo que hay
# que enviar de vuelta.
#
# Nota: adb escribe avisos por stderr y PowerShell 5.1 los convierte en errores
# terminantes. Por eso ErrorActionPreference es Continue y adb se invoca con
# Run-Adb, que normaliza la salida y nunca aborta el diagnostico.

$ErrorActionPreference = "Continue"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$log = Join-Path $here "esp-diagnostico.txt"

function Find-Adb {
    $local = Join-Path $here "platform-tools\adb.exe"
    if (Test-Path $local) { return $local }
    if ($env:ADB -and (Test-Path $env:ADB)) { return $env:ADB }
    $command = Get-Command adb -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }
    return $null
}

function Run-Adb {
    param([string[]]$AdbArgs)
    $previous = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    $raw = & $adb @AdbArgs 2>&1
    $ErrorActionPreference = $previous
    $text = @()
    foreach ($entry in $raw) {
        if ($entry -is [System.Management.Automation.ErrorRecord]) {
            $text += [string]$entry.Exception.Message
        } else {
            $text += [string]$entry
        }
    }
    return $text
}

$adb = Find-Adb
if (-not $adb) {
    Write-Host "ERROR: no encuentro adb. Copia la carpeta platform-tools junto a este archivo." -ForegroundColor Red
    exit 1
}

$target = @()
if ($Serial) { $target = @("-s", $Serial) }

Run-Adb (@("start-server")) | Out-Null

$devices = @(Run-Adb (@("devices")) | Where-Object { $_ -match "\sdevice$" })
if ($devices.Count -eq 0) {
    Write-Host "No hay dispositivo autorizado." -ForegroundColor Yellow
    Write-Host "  1) activa 'Depuracion USB' en Opciones de desarrollador"
    Write-Host "  2) conecta el cable y acepta la huella RSA en el telefono"
    Write-Host "  3) si sigue igual: .\platform-tools\adb.exe kill-server  y repite"
    Run-Adb (@("devices", "-l")) | ForEach-Object { Write-Host "   $_" }
    exit 1
}
Write-Host "Dispositivo: $($devices[0])"

$phoneAbi = ((Run-Adb ($target + @("shell", "getprop ro.product.cpu.abi"))) -join " ").Trim()
if ($Abi -eq "auto") {
    if ($phoneAbi -eq "arm64-v8a") { $Abi = "arm64" }
    elseif ($phoneAbi -eq "x86_64" -or $phoneAbi -eq "x86") { $Abi = "x86_64" }
    else {
        Write-Host "ERROR: ABI '$phoneAbi' no soportada. Se necesita un telefono de 64 bits." -ForegroundColor Red
        exit 1
    }
}
elseif ($Abi -eq "arm64-v8a") { $Abi = "arm64" }

$binary = Join-Path $here "esp_df-$Abi"
if (-not (Test-Path $binary)) {
    Write-Host "ERROR: falta esp_df-$Abi junto a este script (ABI del telefono: $phoneAbi)." -ForegroundColor Red
    exit 1
}

Set-Content -Path $log -Value "=== Diagnostico ESP  $(Get-Date -Format s) ===" -Encoding UTF8
Add-Content -Path $log -Value "abi: $Abi ($phoneAbi)  paquete: $Package" -Encoding UTF8

function Step {
    param([string]$Title, [string]$Command)
    Write-Host ""
    Write-Host "== $Title" -ForegroundColor Cyan
    Add-Content -Path $log -Value "`r`n--- $Title ---" -Encoding UTF8
    Add-Content -Path $log -Value "`$ $Command" -Encoding UTF8
    $out = @(Run-Adb ($target + @("shell", $Command)))
    foreach ($line in $out) {
        if ($line -ne "") { Write-Host "   $line" }
    }
    $out | Add-Content -Path $log -Encoding UTF8
}

Step "modelo y version" "getprop ro.product.model; getprop ro.product.cpu.abi; getprop ro.build.version.release"
Step "modulo del kernel" "lsmod | grep phmeop"
Step "nodo del driver" "ls -l /dev/phmeop"
Step "root" "su -c id"

Write-Host ""
Write-Host "== copiando binario al telefono" -ForegroundColor Cyan
Run-Adb ($target + @("push", $binary, "/data/local/tmp/esp_df")) | Out-Null
Run-Adb ($target + @("shell", "su -c 'chmod 755 /data/local/tmp/esp_df'")) | Out-Null

Step "probe" "su -c '/data/local/tmp/esp_df probe --package $Package'"
Step "test" "su -c '/data/local/tmp/esp_df test --ticks $Ticks --interval 500 --package $Package'"
Step "dump" "su -c '/data/local/tmp/esp_df dump --raw --package $Package'"

Write-Host ""
$lines = @()
if (Test-Path $log) { $lines = @(Get-Content $log) }

$gworld = ($lines | Where-Object { $_ -match "GWorld" } | Select-Object -First 1)
$cam = ($lines | Where-Object { $_ -match "^CAM:" } | Select-Object -First 1)
$targets = ($lines | Where-Object { $_ -match "targets ----" } | Select-Object -First 1)
$enc = ($lines | Where-Object { $_ -match "\[ENC!\]" } | Select-Object -First 1)

Write-Host "== resultado" -ForegroundColor Green
if ($gworld) { Write-Host "   $($gworld.Trim())" }
if ($cam) { Write-Host "   $($cam.Trim())" }
if ($targets) { Write-Host "   $($targets.Trim())" }
if ($enc) { Write-Host "   posiciones cifradas: $($enc.Trim())" -ForegroundColor Yellow }
Write-Host ""
Write-Host "Log completo: $log" -ForegroundColor Green
Write-Host "Envia ese archivo. Si dice 'GWorld no encontrado', entra a una partida y repite."
