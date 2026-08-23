[CmdletBinding()]
param(
    [string]$OutputPath = 'artifacts\quest-screenshot.bmp',
    [int]$TimeoutSeconds = 20
)

$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$adb = 'D:\Android\Sdk\platform-tools\adb.exe'
$package = 'dev.deusex.questvr.smoketest'
$requestPath = 'files/DeusEx/quest-map.request'
$devicePath = "/sdcard/Android/data/$package/files/quest-screenshot.bmp"

if (-not (Test-Path -LiteralPath $adb)) {
    throw "ADB not found: $adb"
}
if ($TimeoutSeconds -lt 1 -or $TimeoutSeconds -gt 120) {
    throw 'TimeoutSeconds must be between 1 and 120.'
}
$devices = & $adb devices
$authorizedDevices = @($devices | Select-Object -Skip 1 | Where-Object { $_ -match '\sdevice$' })
if ($authorizedDevices.Count -ne 1) {
    throw "Expected exactly one authorized Quest device; found $($authorizedDevices.Count)."
}

$destination = if ([System.IO.Path]::IsPathRooted($OutputPath)) {
    [System.IO.Path]::GetFullPath($OutputPath)
} else {
    [System.IO.Path]::GetFullPath((Join-Path $projectRoot $OutputPath))
}
$destinationDirectory = [System.IO.Path]::GetDirectoryName($destination)
if (-not (Test-Path -LiteralPath $destinationDirectory)) {
    New-Item -ItemType Directory -Path $destinationDirectory | Out-Null
}

& $adb shell rm -f $devicePath
if ($LASTEXITCODE -ne 0) {
    throw 'Could not clear the previous app screenshot.'
}
& $adb shell "run-as $package sh -c 'echo SCREENSHOT > $requestPath'"
if ($LASTEXITCODE -ne 0) {
    throw 'Could not request an in-app screenshot.'
}

$deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
$ready = $false
while ([DateTime]::UtcNow -lt $deadline) {
    $result = @(& $adb shell "if [ -s '$devicePath' ]; then echo ready; fi")
    if (($result -join '').Trim() -eq 'ready') {
        $ready = $true
        break
    }
    Start-Sleep -Milliseconds 250
}
if (-not $ready) {
    throw "Screenshot was not produced within $TimeoutSeconds seconds."
}

& $adb pull $devicePath $destination
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $destination)) {
    throw 'ADB could not pull the in-app screenshot.'
}
$stream = [System.IO.File]::OpenRead($destination)
try {
    if ($stream.Length -lt 54 -or $stream.ReadByte() -ne [char]'B' -or
        $stream.ReadByte() -ne [char]'M') {
        throw 'The captured file is not a valid BMP image.'
    }
} finally {
    $stream.Dispose()
}

Get-Item -LiteralPath $destination | Select-Object FullName, Length, LastWriteTime
