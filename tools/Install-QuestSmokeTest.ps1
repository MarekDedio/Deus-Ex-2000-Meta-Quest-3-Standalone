[CmdletBinding()]
param(
    [string]$AdbPath = 'D:\Android\Sdk\platform-tools\adb.exe',
    [string]$ApkPath,
    [ValidatePattern('^[A-Za-z0-9_.:-]+$')][string]$DeviceSerial
)

$ErrorActionPreference = 'Stop'
if (Test-Path variable:PSNativeCommandUseErrorActionPreference) {
    $PSNativeCommandUseErrorActionPreference = $false
}
if (-not $ApkPath) {
    $ApkPath = Join-Path $PSScriptRoot '..\android\build\outputs\apk\debug\DeusExQuestVrSmokeTest-debug.apk'
}

if (-not (Test-Path -LiteralPath $AdbPath -PathType Leaf)) {
    throw "ADB not found: $AdbPath"
}
if (-not (Test-Path -LiteralPath $ApkPath -PathType Leaf)) {
    throw "APK not found. Run Build-QuestSmokeTest.ps1 first."
}

$devices = @(& $AdbPath devices)
if ($LASTEXITCODE -ne 0) { throw 'Could not list ADB devices.' }
$authorizedDevices = @($devices | Select-Object -Skip 1 |
    Where-Object { $_ -match '^\S+\s+device\s*$' })
if ($DeviceSerial) {
    if (-not ($authorizedDevices | Where-Object { ($_ -split '\s+')[0] -ceq $DeviceSerial })) {
        throw "Quest device is not connected and authorized: $DeviceSerial"
    }
} else {
    if ($authorizedDevices.Count -ne 1) {
        throw "Expected exactly one authorized Quest device; found $($authorizedDevices.Count)."
    }
    $DeviceSerial = ($authorizedDevices[0] -split '\s+')[0]
}

# Replace the installed APK without uninstalling or clearing its saves/data.
# Pin every operation to this serial, even if another device connects later.
& $AdbPath -s $DeviceSerial install -r $ApkPath
if ($LASTEXITCODE -ne 0) {
    throw "ADB install failed with exit code $LASTEXITCODE"
}

& $AdbPath -s $DeviceSerial shell am force-stop dev.deusex.questvr.smoketest
if ($LASTEXITCODE -ne 0) { throw 'Could not stop the previous app instance after installation.' }
& (Join-Path $PSScriptRoot 'Launch-QuestSmokeTest.ps1') -AdbPath $AdbPath -DeviceSerial $DeviceSerial
