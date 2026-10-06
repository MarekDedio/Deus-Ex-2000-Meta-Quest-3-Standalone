[CmdletBinding()]
param(
    [string]$OutputPath = 'artifacts\quest-screenshot.bmp',
    [int]$TimeoutSeconds = 20,
    [string]$AdbPath = 'D:\Android\Sdk\platform-tools\adb.exe'
)

$ErrorActionPreference = 'Stop'
if (Test-Path variable:PSNativeCommandUseErrorActionPreference) {
    $PSNativeCommandUseErrorActionPreference = $false
}
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$adb = $AdbPath
$package = 'dev.deusex.questvr.smoketest'
$devicePath = "/sdcard/Android/data/$package/files/quest-screenshot.bmp"
$requestHelper = Join-Path $PSScriptRoot 'Send-QuestDiagnostic.ps1'

if (-not (Test-Path -LiteralPath $adb)) {
    throw "ADB not found: $adb"
}
if (-not (Test-Path -LiteralPath $requestHelper -PathType Leaf)) {
    throw "Diagnostic request helper not found: $requestHelper"
}
if ($TimeoutSeconds -lt 1 -or $TimeoutSeconds -gt 120) {
    throw 'TimeoutSeconds must be between 1 and 120.'
}
$devices = @(& $adb devices)
if ($LASTEXITCODE -ne 0) { throw 'Could not list ADB devices.' }
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

$validCapture = $false
for ($attempt = 1; $attempt -le 3 -and -not $validCapture; $attempt++) {
    & $adb shell rm -f $devicePath
    if ($LASTEXITCODE -ne 0) {
        throw 'Could not clear the previous app screenshot.'
    }
    & $requestHelper -Command SCREENSHOT -TimeoutSeconds $TimeoutSeconds -AdbPath $adb | Out-Null

    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    $ready = $false
    while ([DateTime]::UtcNow -lt $deadline) {
        $result = @(& $adb shell "if [ -s '$devicePath' ]; then echo ready; fi")
        if ($LASTEXITCODE -ne 0) { throw 'Could not inspect the app screenshot readiness.' }
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
    $bytes = [System.IO.File]::ReadAllBytes($destination)
    if ($bytes.Length -lt 54 -or $bytes[0] -ne [char]'B' -or $bytes[1] -ne [char]'M') {
        throw 'The captured file is not a valid BMP image.'
    }
    $sampledColors = [System.Collections.Generic.HashSet[uint32]]::new()
    # BMP rows are bottom-up. Sampling only the lower half prevents HUD glyphs
    # from making an otherwise uniform compositor/readback failure look valid.
    $sampleLimit = 54 + [int][Math]::Floor(($bytes.Length - 54) / 2)
    $sampleStride = [Math]::Max(4, [int][Math]::Floor(($sampleLimit - 54) / 4096 / 4) * 4)
    for ($offset = 54; $offset + 3 -lt $sampleLimit; $offset += $sampleStride) {
        $color = [uint32]$bytes[$offset] -bor
            ([uint32]$bytes[$offset + 1] -shl 8) -bor
            ([uint32]$bytes[$offset + 2] -shl 16)
        [void]$sampledColors.Add($color)
        if ($sampledColors.Count -ge 8) {
            $validCapture = $true
            break
        }
    }
    if (-not $validCapture) {
        Write-Warning "Capture attempt $attempt was nearly uniform; retrying."
        Start-Sleep -Milliseconds 750
    }
}
if (-not $validCapture) {
    throw 'Quest returned three nearly uniform in-app frames.'
}

Get-Item -LiteralPath $destination | Select-Object FullName, Length, LastWriteTime
