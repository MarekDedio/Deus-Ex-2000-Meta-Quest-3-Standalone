[CmdletBinding(DefaultParameterSetName = 'Command')]
param(
    [Parameter(Mandatory = $true, ParameterSetName = 'Command')]
    [ValidateSet('MENU', 'PAGE', 'TURNLEFT', 'TURNRIGHT', 'PICKUP', 'SCREENSHOT')]
    [string]$Command,
    [Parameter(Mandatory = $true, ParameterSetName = 'Map')]
    [ValidatePattern('^[A-Za-z0-9_-]+$')]
    [string]$MapName,
    [ValidateRange(1, 120)][int]$TimeoutSeconds = 20,
    [string]$AdbPath = 'D:\Android\Sdk\platform-tools\adb.exe'
)

$ErrorActionPreference = 'Stop'
if (Test-Path variable:PSNativeCommandUseErrorActionPreference) {
    $PSNativeCommandUseErrorActionPreference = $false
}
if (-not (Test-Path -LiteralPath $AdbPath -PathType Leaf)) {
    throw "ADB not found: $AdbPath"
}
$devices = @(& $AdbPath devices)
if ($LASTEXITCODE -ne 0) { throw 'Could not list ADB devices.' }
$authorizedDevices = @($devices | Select-Object -Skip 1 |
    Where-Object { $_ -match '^\S+\s+device\s*$' })
if ($authorizedDevices.Count -ne 1) {
    throw "Expected exactly one authorized device; found $($authorizedDevices.Count)."
}
$deviceSerial = ($authorizedDevices[0] -split '\s+')[0]
$package = 'dev.deusex.questvr.smoketest'
$requestPath = 'files/DeusEx/quest-map.request'
$request = if ($PSCmdlet.ParameterSetName -eq 'Map') { $MapName } else { $Command.ToUpperInvariant() }

function Wait-EmptyRequestMailbox {
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    do {
        $state = @(& $AdbPath -s $deviceSerial shell "run-as $package sh -c 'if [ -e $requestPath ]; then echo pending; else echo clear; fi'")
        if ($LASTEXITCODE -ne 0) { throw 'Could not inspect the app diagnostic mailbox.' }
        $stateText = ($state -join '').Trim()
        if ($stateText -eq 'clear') { return }
        if ($stateText -ne 'pending') { throw "Unexpected diagnostic mailbox state: $stateText" }
        Start-Sleep -Milliseconds 200
    } while ([DateTime]::UtcNow -lt $deadline)
    throw "The app has not consumed its pending diagnostic within $TimeoutSeconds seconds. It was left intact. Check app/tracking state before retrying."
}

if ($MapName) {
    & $AdbPath -s $deviceSerial shell run-as $package test -f "files/DeusEx/Maps/$MapName.dx"
    if ($LASTEXITCODE -ne 0) { throw "The original map is not deployed: $MapName" }
}
Wait-EmptyRequestMailbox
# noclobber prevents two callers from replacing each other's pending request.
# Both command and map names are restricted to shell-safe tokens above.
& $AdbPath -s $deviceSerial shell "run-as $package sh -c 'set -C; echo $request > $requestPath'"
if ($LASTEXITCODE -ne 0) {
    throw 'Could not queue the diagnostic; any existing pending request was preserved.'
}
Wait-EmptyRequestMailbox
[pscustomobject]@{
    Device = $deviceSerial
    Request = $request
    Consumed = $true
    Completion = 'Request consumed only. Map uploads and screenshot rendering may still be in progress.'
}
