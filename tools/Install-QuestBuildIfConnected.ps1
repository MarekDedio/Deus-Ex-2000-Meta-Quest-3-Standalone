[CmdletBinding()]
param(
    [string]$AdbPath = 'D:\Android\Sdk\platform-tools\adb.exe',
    [Parameter(Mandatory = $true)][string]$ApkPath,
    [ValidatePattern('^[A-Za-z0-9_.:-]+$')][string]$DeviceSerial,
    [switch]$SkipInstall,
    [switch]$ValidateOnly
)

$ErrorActionPreference = 'Stop'
if (Test-Path variable:PSNativeCommandUseErrorActionPreference) {
    $PSNativeCommandUseErrorActionPreference = $false
}
function Skip-Deployment([string]$Reason) {
    Write-Host "Automatic Quest install skipped: $Reason"
    [pscustomobject]@{ Status = 'Skipped'; Device = $DeviceSerial; ApkPath = $ApkPath; Reason = $Reason }
}
if ($ValidateOnly) { Skip-Deployment 'Validation-only mode never contacts a device.'; return }
if ($SkipInstall) { Skip-Deployment 'Explicit -SkipInstall (offline/CI build).'; return }
if (-not (Test-Path -LiteralPath $ApkPath -PathType Leaf)) { throw "Built APK not found: $ApkPath" }
if (-not (Test-Path -LiteralPath $AdbPath -PathType Leaf)) {
    Skip-Deployment "ADB unavailable at '$AdbPath'; APK build is retained."; return
}
if (-not $DeviceSerial) { $DeviceSerial = '2G0YC5ZG620985' }
# Never auto-select another authorized phone/headset just because it is the
# only ADB device. Another Quest requires the explicit serial override.
$deviceLines = @(& $AdbPath devices)
if ($LASTEXITCODE -ne 0) { Skip-Deployment 'ADB device enumeration failed; no installation attempted.'; return }
$matchingDevices = @($deviceLines | Where-Object {
    $_ -match '^\s*(\S+)\s+(device|offline|unauthorized)\s*$' -and $Matches[1] -ceq $DeviceSerial
})
if ($matchingDevices.Count -ne 1) {
    Skip-Deployment "Expected Quest '$DeviceSerial' is absent or ambiguous; no alternate device selected. Use -DeviceSerial for another Quest 3."; return
}
$deviceState = ($matchingDevices[0].Trim() -split '\s+')[-1]
if ($deviceState -cne 'device') {
    Skip-Deployment "Quest '$DeviceSerial' is $deviceState; USB authorization/connection is required."; return
}
$modelOutput = @(& $AdbPath -s $DeviceSerial shell getprop ro.product.model)
$modelExitCode = $LASTEXITCODE
$model = ($modelOutput -join '').Trim()
if ($modelExitCode -ne 0 -or $model -notmatch '^(Meta )?Quest[ _]3$') {
    Skip-Deployment "Target '$DeviceSerial' could not be verified as Quest 3 (model '$model', exit $modelExitCode); no installation attempted."; return
}
# The manual helper revalidates the serial/model immediately before installing.
# Any failure after choosing an authorized Quest propagates visibly to the
# caller; a successful compile must never be reported as a successful install.
$installed = & (Join-Path $PSScriptRoot 'Install-QuestSmokeTest.ps1') `
    -AdbPath $AdbPath -ApkPath $ApkPath -DeviceSerial $DeviceSerial -InstallOnly
[pscustomobject]@{
    Status = 'Installed'; Device = $installed.Device; ApkPath = $installed.ApkPath
    ApkSha256 = $installed.ApkSha256; Reason = $installed.State
}
