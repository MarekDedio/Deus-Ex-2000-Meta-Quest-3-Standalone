[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$sdkRoot = Join-Path $projectRoot 'third_party\Meta-OpenXR-SDK'
$patchPath = Join-Path $projectRoot 'patches\meta-openxr-tinyui-font-override.patch'

if (-not (Test-Path -LiteralPath (Join-Path $sdkRoot '.git'))) {
    throw "Missing pinned Meta OpenXR SDK checkout: $sdkRoot"
}

& git -C $sdkRoot apply --reverse --check $patchPath 2>$null
if ($LASTEXITCODE -eq 0) {
    Write-Host 'Meta OpenXR SDK patch already applied.'
    return
}

& git -C $sdkRoot apply --check $patchPath
if ($LASTEXITCODE -ne 0) {
    throw 'The Meta OpenXR SDK checkout does not match the pinned revision or has conflicting edits.'
}

& git -C $sdkRoot apply $patchPath
if ($LASTEXITCODE -ne 0) {
    throw "Failed to apply $patchPath"
}
Write-Host 'Applied Meta OpenXR SDK TinyUI font override patch.'
