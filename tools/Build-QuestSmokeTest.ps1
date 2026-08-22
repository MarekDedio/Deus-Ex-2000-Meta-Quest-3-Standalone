[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$androidRoot = Join-Path $projectRoot 'android'
$sdkRoot = 'D:\Android\Sdk'
$jdkRoot = 'C:\Program Files\Microsoft\jdk-17.0.20.8-hotspot'

$required = @(
    (Join-Path $jdkRoot 'bin\java.exe'),
    (Join-Path $sdkRoot 'platforms\android-32\android.jar'),
    (Join-Path $sdkRoot 'ndk\27.0.12077973\source.properties'),
    (Join-Path $androidRoot 'gradlew.bat')
)

foreach ($path in $required) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Missing build dependency: $path"
    }
}

& (Join-Path $PSScriptRoot 'Apply-ThirdPartyPatches.ps1')
if ($LASTEXITCODE -ne 0) {
    throw "Third-party patch preparation failed with exit code $LASTEXITCODE"
}

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
}

$apk = Join-Path $androidRoot 'build\outputs\apk\debug\DeusExQuestVrSmokeTest-debug.apk'
if (-not (Test-Path -LiteralPath $apk)) {
    throw "Build completed without the expected APK: $apk"
}

Get-Item -LiteralPath $apk | Select-Object FullName, Length, LastWriteTime
