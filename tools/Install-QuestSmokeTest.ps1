[CmdletBinding()]
param(
    [string]$AdbPath = 'D:\Android\Sdk\platform-tools\adb.exe',
    [string]$ApkPath,
    [ValidatePattern('^[A-Za-z0-9_.:-]+$')][string]$DeviceSerial,
    [switch]$InstallOnly
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
$modelOutput = @(& $AdbPath -s $DeviceSerial shell getprop ro.product.model)
$modelExitCode = $LASTEXITCODE
$model = ($modelOutput -join '').Trim()
if ($modelExitCode -ne 0 -or $model -notmatch '^(Meta )?Quest[ _]3$') {
    throw "Selected device is not a verified Quest 3: $DeviceSerial (model '$model', exit $modelExitCode)."
}
$installOutput = @(& $AdbPath -s $DeviceSerial install -r $ApkPath)
$installExitCode = $LASTEXITCODE
if ($installExitCode -ne 0 -or -not ($installOutput | Where-Object { $_ -match '^Success\s*$' })) {
    throw "ADB install failed with exit code ${installExitCode}: $($installOutput -join ' ')"
}
Write-Host "Installed APK on Quest 3 $DeviceSerial using replace-install; app data was not cleared."
if ($InstallOnly) {
    [pscustomobject]@{
        Device = $DeviceSerial
        ApkPath = (Resolve-Path -LiteralPath $ApkPath).Path
        ApkSha256 = (Get-FileHash -LiteralPath $ApkPath -Algorithm SHA256).Hash
        State = 'Installed; app not launched. Rendering/tracking/gameplay validation is separate.'
    }
    return
}

& $AdbPath -s $DeviceSerial shell am force-stop dev.deusex.questvr.smoketest
if ($LASTEXITCODE -ne 0) { throw 'Could not stop the previous app instance after installation.' }
& (Join-Path $PSScriptRoot 'Launch-QuestSmokeTest.ps1') -AdbPath $AdbPath -DeviceSerial $DeviceSerial
