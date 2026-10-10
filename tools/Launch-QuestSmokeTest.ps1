[CmdletBinding()]
param(
    [string]$AdbPath = 'D:\Android\Sdk\platform-tools\adb.exe',
    [ValidatePattern('^[A-Za-z0-9_.:-]+$')][string]$DeviceSerial,
    [ValidateRange(1, 60)][int]$StartupTimeoutSeconds = 10
)

$ErrorActionPreference = 'Stop'
if (Test-Path variable:PSNativeCommandUseErrorActionPreference) {
    $PSNativeCommandUseErrorActionPreference = $false
}
if (-not (Test-Path -LiteralPath $AdbPath -PathType Leaf)) {
    throw "ADB not found: $AdbPath"
}
$questDevices = @(& $AdbPath devices)
if ($LASTEXITCODE -ne 0) { throw 'Could not list ADB devices.' }
$questAuthorized = @($questDevices | Select-Object -Skip 1 |
    Where-Object { $_ -match '^\S+\s+device\s*$' })
if ($DeviceSerial) {
    if (-not ($questAuthorized | Where-Object { ($_ -split '\s+')[0] -ceq $DeviceSerial })) {
        throw "Quest device is not connected and authorized: $DeviceSerial"
    }
} else {
    if ($questAuthorized.Count -ne 1) {
        throw "Expected exactly one authorized device; found $($questAuthorized.Count)."
    }
    $DeviceSerial = ($questAuthorized[0] -split '\s+')[0]
}

$questPackage = 'dev.deusex.questvr.smoketest'
# Exactly one launch intent. The wait distinguishes startup from an immediate
# pidof miss; repeated intents during initialization can cause a focus cycle.
$questLaunch = @(& $AdbPath -s $DeviceSerial shell am start -W -n "$questPackage/dev.deusex.questvr.MainActivity")
if ($LASTEXITCODE -ne 0 -or -not ($questLaunch | Where-Object { $_ -match '^Status:\s+ok\s*$' })) {
    throw "Quest launch failed: $($questLaunch -join ' ')"
}
$questDeadline = [DateTime]::UtcNow.AddSeconds($StartupTimeoutSeconds)
do {
    $questProcessIds = @(& $AdbPath -s $DeviceSerial shell pidof $questPackage)
    if ($LASTEXITCODE -eq 0 -and ($questProcessIds -join '').Trim() -match '^\d+(\s+\d+)*$') {
        [pscustomobject]@{
            Device = $DeviceSerial
            Package = $questPackage
            ProcessIds = ($questProcessIds -join '').Trim()
            State = 'Activity launched and process running; map/tracking readiness is separate.'
        }
        return
    }
    Start-Sleep -Milliseconds 250
} while ([DateTime]::UtcNow -lt $questDeadline)
throw 'The activity launch returned success, but no running app process was confirmed. Inspect logcat before relaunching.'
