[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$adb = 'D:\Android\Sdk\platform-tools\adb.exe'
$apk = Join-Path $projectRoot 'android\build\outputs\apk\debug\DeusExQuestVrSmokeTest-debug.apk'

if (-not (Test-Path -LiteralPath $adb)) {
    throw "ADB not found: $adb"
}
if (-not (Test-Path -LiteralPath $apk)) {
    throw "APK not found. Run Build-QuestSmokeTest.ps1 first."
}

$devices = & $adb devices
$authorizedDevices = @($devices | Select-Object -Skip 1 | Where-Object { $_ -match '\sdevice$' })
if ($authorizedDevices.Count -ne 1) {
    throw "Expected exactly one authorized Quest device; found $($authorizedDevices.Count)."
}

& $adb install -r $apk
if ($LASTEXITCODE -ne 0) {
    throw "ADB install failed with exit code $LASTEXITCODE"
}

& $adb shell am force-stop dev.deusex.questvr.smoketest
& $adb shell am start -n dev.deusex.questvr.smoketest/dev.deusex.questvr.MainActivity
