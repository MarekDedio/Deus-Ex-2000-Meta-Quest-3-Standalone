[CmdletBinding()]
param(
    [Parameter()]
    [string]$GameRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
)

$ErrorActionPreference = 'Stop'
$adb = 'D:\Android\Sdk\platform-tools\adb.exe'
$packageName = 'dev.deusex.questvr.smoketest'
$remoteRoot = 'files/DeusEx'
$remoteArchive = '/data/local/tmp/deusex-quest-data.tar'
$stagingRoot = 'D:\Android\Staging'
$localArchive = Join-Path $stagingRoot 'deusex-quest-data.tar'
$dataDirectories = @('Maps', 'Music', 'Sounds', 'System', 'Textures')

if (-not (Test-Path -LiteralPath $adb)) {
    throw "ADB not found: $adb"
}

$devices = & $adb devices
$authorizedDevices = @($devices | Select-Object -Skip 1 | Where-Object { $_ -match '\sdevice$' })
if ($authorizedDevices.Count -ne 1) {
    throw "Expected exactly one authorized Quest device; found $($authorizedDevices.Count)."
}

& (Join-Path $PSScriptRoot 'Test-GameData.ps1') -GameRoot $GameRoot | Out-Host
if ($LASTEXITCODE -ne 0) {
    throw 'The source installation failed validation.'
}

New-Item -ItemType Directory -Force -Path $stagingRoot | Out-Null
$resolvedArchive = [IO.Path]::GetFullPath($localArchive)
if (-not $resolvedArchive.StartsWith("$stagingRoot\", [StringComparison]::OrdinalIgnoreCase)) {
    throw "Unsafe staging path: $resolvedArchive"
}
if (Test-Path -LiteralPath $resolvedArchive) {
    Remove-Item -LiteralPath $resolvedArchive -Force
}

try {
    Write-Host 'Creating temporary game-data archive...'
    & tar.exe -cf $resolvedArchive -C $GameRoot @dataDirectories
    if ($LASTEXITCODE -ne 0) {
        throw 'Could not create the game-data archive.'
    }

    Write-Host 'Transferring archive to Quest...'
    & $adb push $resolvedArchive $remoteArchive
    if ($LASTEXITCODE -ne 0) {
        throw 'ADB archive transfer failed.'
    }
    & $adb shell chmod 644 $remoteArchive

    Write-Host 'Extracting with the application UID...'
    & $adb shell run-as $packageName mkdir -p $remoteRoot
    if ($LASTEXITCODE -ne 0) {
        throw "Could not create internal app directory $remoteRoot"
    }
    & $adb shell run-as $packageName tar -xf $remoteArchive -C $remoteRoot
    if ($LASTEXITCODE -ne 0) {
        throw 'Could not extract game data into internal app storage.'
    }

    Write-Host 'Verifying required files on Quest...'
    $requiredRemoteFiles = @(
        "$remoteRoot/System/DeusEx.u",
        "$remoteRoot/Maps/00_Training.dx",
        "$remoteRoot/Textures/CoreTexMetal.utx",
        "$remoteRoot/Music/Training_Music.umx"
    )

    foreach ($path in $requiredRemoteFiles) {
        & $adb shell run-as $packageName test -f $path
        if ($LASTEXITCODE -ne 0) {
            throw "Missing deployed file: $path"
        }
    }

    & $adb shell run-as $packageName du -sh $remoteRoot
} finally {
    & $adb shell rm -f $remoteArchive | Out-Null
    if (Test-Path -LiteralPath $resolvedArchive) {
        Remove-Item -LiteralPath $resolvedArchive -Force
    }
}
