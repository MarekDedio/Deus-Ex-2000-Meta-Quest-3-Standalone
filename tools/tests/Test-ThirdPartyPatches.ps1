[CmdletBinding()]
param(
    [string]$SdkRoot
)

# Exercise the real patch helper on a minimal, isolated copy of pinned SDK
# source. Never modify the developer's SDK checkout and never contact a device.
$ErrorActionPreference = 'Stop'
if (Test-Path variable:PSNativeCommandUseErrorActionPreference) {
    $PSNativeCommandUseErrorActionPreference = $false
}
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if (-not $SdkRoot) {
    $SdkRoot = Join-Path $projectRoot 'third_party\Meta-OpenXR-SDK'
}
$gitCommand = Get-Command git -CommandType Application -ErrorAction Stop | Select-Object -First 1
$tarCommand = Get-Command tar -CommandType Application -ErrorAction Stop | Select-Object -First 1
if (-not (Test-Path -LiteralPath (Join-Path $SdkRoot '.git'))) {
    throw "Missing pinned Meta OpenXR SDK checkout: $SdkRoot"
}
$fixtureRoot = Join-Path ([IO.Path]::GetTempPath()) ('QuestPatchTests-' + [guid]::NewGuid())
$fixtureSdk = Join-Path $fixtureRoot 'third_party\Meta-OpenXR-SDK'
New-Item -ItemType Directory -Path $fixtureSdk, (Join-Path $fixtureRoot 'tools'), (Join-Path $fixtureRoot 'patches') | Out-Null
$fixtureHelper = Join-Path $fixtureRoot 'tools\Apply-ThirdPartyPatches.ps1'
$patchNames = @('meta-openxr-tinyui-font-override.patch', 'meta-openxr-session-lifecycle.patch')
$passed = 0

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw "Assertion failed: $Message" }
}
function Complete-Test([string]$Name) {
    $script:passed++
    Write-Output "PASS: $Name"
}
function Get-FixtureHashes {
    foreach ($relativePath in @(
        'Samples\SampleXrFramework\Src\Input\TinyUI.cpp',
        'Samples\SampleXrFramework\Src\Input\TinyUI.h',
        'Samples\SampleXrFramework\Src\XrApp.cpp'
    )) {
        (Get-FileHash -LiteralPath (Join-Path $fixtureSdk $relativePath) -Algorithm SHA256).Hash
    }
}

try {
    Copy-Item -LiteralPath (Join-Path $projectRoot 'tools\Apply-ThirdPartyPatches.ps1') -Destination $fixtureHelper
    foreach ($patchName in $patchNames) {
        Copy-Item -LiteralPath (Join-Path $projectRoot ('patches\' + $patchName)) -Destination (Join-Path $fixtureRoot 'patches')
    }
    $archivePath = Join-Path $fixtureRoot 'pinned-sdk-source.tar'
    & $gitCommand.Source -C $SdkRoot archive ('--output=' + $archivePath) HEAD -- `
        Samples/SampleXrFramework/Src/XrApp.cpp `
        Samples/SampleXrFramework/Src/Input/TinyUI.cpp `
        Samples/SampleXrFramework/Src/Input/TinyUI.h
    Assert-True ($LASTEXITCODE -eq 0) 'read-only pinned SDK archive succeeds'
    & $tarCommand.Source -xf $archivePath -C $fixtureSdk
    Assert-True ($LASTEXITCODE -eq 0) 'archive extraction into isolated fixture succeeds'
    & $gitCommand.Source init --quiet $fixtureSdk
    Assert-True ($LASTEXITCODE -eq 0) 'isolated fixture repository initialized'

    & $fixtureHelper
    foreach ($patchName in $patchNames) {
        & $gitCommand.Source -C $fixtureSdk apply --reverse --check (Join-Path $fixtureRoot ('patches\' + $patchName))
        Assert-True ($LASTEXITCODE -eq 0) "fresh helper applied $patchName"
    }
    Complete-Test 'fresh pinned SDK source accepts both real patches'

    $patchedHashes = @(Get-FixtureHashes) -join ','
    & $fixtureHelper
    Assert-True ((@(Get-FixtureHashes) -join ',') -ceq $patchedHashes) 'second application changes no patched source bytes'
    Complete-Test 'already-applied patches are idempotent'

    $xrAppPath = Join-Path $fixtureSdk 'Samples\SampleXrFramework\Src\XrApp.cpp'
    $xrApp = Get-Content -LiteralPath $xrAppPath -Raw
    Assert-True ($xrApp.Contains('assert(SessionActive == false);') -and $xrApp.Contains('assert(SessionActive);')) 'OpenXR active-session assertions retained'
    Assert-True ($xrApp.Contains('OXR(result = xrBeginSession(Session, &sessionBeginInfo));') -and $xrApp.Contains('OXR(xrEndSession(Session));')) 'OpenXR begin/end calls and error checks retained'
    Assert-True (-not $xrApp.Contains('assert(Resumed);') -and -not $xrApp.Contains('assert(Resumed == false);')) 'only Android queue timing assumptions are relaxed'
    Complete-Test 'lifecycle patch preserves OpenXR session state contracts'

    # Create a conflicting, local-only fixture edit. This generated test source
    # is never installed, built, committed or written into the actual SDK.
    & $gitCommand.Source -C $fixtureSdk apply --reverse (Join-Path $fixtureRoot 'patches\meta-openxr-session-lifecycle.patch')
    Assert-True ($LASTEXITCODE -eq 0) 'fixture lifecycle patch can be reversed'
    $xrApp = Get-Content -LiteralPath $xrAppPath -Raw
    Assert-True ($xrApp.Contains('assert(Resumed);')) 'conflict fixture starts from original timing assertion'
    $xrApp.Replace('assert(Resumed);', 'assert(Resumed && false);') |
        Set-Content -LiteralPath $xrAppPath -NoNewline -Encoding utf8
    $conflictHashes = @(Get-FixtureHashes) -join ','
    $caught = $null
    try { & $fixtureHelper } catch { $caught = $_ }
    Assert-True ($null -ne $caught -and $caught.Exception.Message -match 'conflicting edits.*session-lifecycle') 'conflict raises explicit patch error'
    Assert-True ((@(Get-FixtureHashes) -join ',') -ceq $conflictHashes) 'conflict leaves all local fixture edits untouched'
    Complete-Test 'conflicting local edits fail closed and remain byte-identical'

    [pscustomobject]@{ Passed = $passed; Failed = 0; HardwareUsed = $false; ActualSdkModified = $false }
} finally {
    $resolvedFixture = [IO.Path]::GetFullPath($fixtureRoot)
    $resolvedTemp = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\') + '\'
    if (-not $resolvedFixture.StartsWith($resolvedTemp, [StringComparison]::OrdinalIgnoreCase) -or
        [IO.Path]::GetFileName($resolvedFixture) -notlike 'QuestPatchTests-*') {
        throw "Refusing to clean unexpected fixture location: $resolvedFixture"
    }
    Remove-Item -LiteralPath $resolvedFixture -Recurse -Force
}
