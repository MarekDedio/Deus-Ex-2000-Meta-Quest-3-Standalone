[CmdletBinding()]
param(
    [string]$AndroidSdkPath,
    [string]$JavaHome,
    [switch]$ValidateOnly
)

$ErrorActionPreference = 'Stop'
if (Test-Path variable:PSNativeCommandUseErrorActionPreference) {
    $PSNativeCommandUseErrorActionPreference = $false
}
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$androidRoot = Join-Path $projectRoot 'android'

function Test-AndroidSdk {
    param([string]$Candidate)
    if (-not $Candidate -or -not (Test-Path -LiteralPath $Candidate -PathType Container)) { return $false }
    return (Test-Path -LiteralPath (Join-Path $Candidate 'platforms\android-32\android.jar')) -and
        (Test-Path -LiteralPath (Join-Path $Candidate 'ndk\27.0.12077973\source.properties')) -and
        (Test-Path -LiteralPath (Join-Path $Candidate 'cmake\3.22.1\bin\cmake.exe'))
}

function Test-Jdk17 {
    param([string]$Candidate)
    if (-not $Candidate) { return $false }
    $java = Join-Path $Candidate 'bin\java.exe'
    if (-not (Test-Path -LiteralPath $java) -or -not (Test-Path -LiteralPath (Join-Path $Candidate 'bin\javac.exe'))) { return $false }
    $ErrorActionPreference = 'Continue'
    $versionOutput = @(& $java -version 2>&1)
    $javaExitCode = $LASTEXITCODE
    return $javaExitCode -eq 0 -and ($versionOutput -join "`n") -match 'version\s+"17[.\-"]'
}

$sdkCandidates = @($env:ANDROID_SDK_ROOT, $env:ANDROID_HOME)
$localProperties = Join-Path $androidRoot 'local.properties'
if (Test-Path -LiteralPath $localProperties) {
    $sdkProperty = Get-Content -LiteralPath $localProperties | Where-Object { $_ -match '^\s*sdk\.dir\s*=' } | Select-Object -Last 1
    if ($sdkProperty) {
        $sdkCandidates += (($sdkProperty -replace '^\s*sdk\.dir\s*=\s*', '') -replace '\\([:= ])', '$1' -replace '\\\\', '\')
    }
}
if ($env:LOCALAPPDATA) { $sdkCandidates += Join-Path $env:LOCALAPPDATA 'Android\Sdk' }
foreach ($drive in Get-PSDrive -PSProvider FileSystem) {
    $sdkCandidates += Join-Path $drive.Root 'Android\Sdk'
}
$adbCommand = Get-Command adb -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
if ($adbCommand) { $sdkCandidates += Split-Path (Split-Path $adbCommand.Source -Parent) -Parent }
if ($AndroidSdkPath) {
    if (-not (Test-AndroidSdk $AndroidSdkPath)) {
        throw "The specified Android SDK is incomplete: $AndroidSdkPath. Required: platform android-32, NDK 27.0.12077973, CMake 3.22.1."
    }
    $sdkRoot = (Resolve-Path -LiteralPath $AndroidSdkPath).Path
} else {
    $sdkRoot = $sdkCandidates | Where-Object { Test-AndroidSdk $_ } | Select-Object -First 1
    if (-not $sdkRoot) {
        throw 'No complete Android SDK found. Install platform android-32, NDK 27.0.12077973 and CMake 3.22.1, then pass -AndroidSdkPath or set ANDROID_SDK_ROOT.'
    }
    $sdkRoot = (Resolve-Path -LiteralPath $sdkRoot).Path
}

$jdkCandidates = @($env:JAVA_HOME)
$microsoftJdkRoot = Join-Path $env:ProgramFiles 'Microsoft'
if (Test-Path -LiteralPath $microsoftJdkRoot) {
    $jdkCandidates += @(Get-ChildItem -LiteralPath $microsoftJdkRoot -Directory -Filter 'jdk-17*' |
        Sort-Object LastWriteTime -Descending | ForEach-Object { $_.FullName })
}
$javaCommand = Get-Command java -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
if ($javaCommand) { $jdkCandidates += Split-Path (Split-Path $javaCommand.Source -Parent) -Parent }
if ($JavaHome) {
    if (-not (Test-Jdk17 $JavaHome)) { throw "The specified Java home is not a working JDK 17: $JavaHome" }
    $jdkRoot = (Resolve-Path -LiteralPath $JavaHome).Path
} else {
    $jdkRoot = $jdkCandidates | Where-Object { Test-Jdk17 $_ } | Select-Object -First 1
    if (-not $jdkRoot) { throw 'No working JDK 17 found. Install Microsoft OpenJDK 17, pass -JavaHome, or set JAVA_HOME.' }
    $jdkRoot = (Resolve-Path -LiteralPath $jdkRoot).Path
}

$required = @(
    (Join-Path $jdkRoot 'bin\java.exe'),
    (Join-Path $sdkRoot 'platforms\android-32\android.jar'),
    (Join-Path $sdkRoot 'ndk\27.0.12077973\source.properties'),
    (Join-Path $sdkRoot 'cmake\3.22.1\bin\cmake.exe'),
    (Join-Path $androidRoot 'gradlew.bat')
)

foreach ($path in $required) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Missing build dependency: $path"
    }
}

Write-Host "Android SDK: $sdkRoot"
Write-Host "JDK 17: $jdkRoot"
& (Join-Path $PSScriptRoot 'Initialize-ThirdParty.ps1')
& (Join-Path $PSScriptRoot 'Apply-ThirdPartyPatches.ps1')
if ($ValidateOnly) {
    Write-Host 'Build dependencies verified and third-party patches ready. Gradle was not run.'
    return
}

$previousJavaHome = $env:JAVA_HOME
$previousAndroidHome = $env:ANDROID_HOME
$previousAndroidSdkRoot = $env:ANDROID_SDK_ROOT
$env:JAVA_HOME = $jdkRoot
$env:ANDROID_HOME = $sdkRoot
$env:ANDROID_SDK_ROOT = $sdkRoot

Push-Location $androidRoot
try {
    & .\gradlew.bat assembleDebug --no-daemon
    if ($LASTEXITCODE -ne 0) {
        throw "Gradle failed with exit code $LASTEXITCODE"
    }
} finally {
    Pop-Location
    $env:JAVA_HOME = $previousJavaHome
    $env:ANDROID_HOME = $previousAndroidHome
    $env:ANDROID_SDK_ROOT = $previousAndroidSdkRoot
}

$apk = Join-Path $androidRoot 'build\outputs\apk\debug\DeusExQuestVrSmokeTest-debug.apk'
if (-not (Test-Path -LiteralPath $apk)) {
    throw "Build completed without the expected APK: $apk"
}

Get-Item -LiteralPath $apk | Select-Object FullName, Length, LastWriteTime
