[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
if (Test-Path variable:PSNativeCommandUseErrorActionPreference) {
    $PSNativeCommandUseErrorActionPreference = $false
}
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$sdkRoot = Join-Path $projectRoot 'third_party\Meta-OpenXR-SDK'
$patchNames = @(
    'meta-openxr-tinyui-font-override.patch',
    'meta-openxr-session-lifecycle.patch'
)

if (-not (Test-Path -LiteralPath (Join-Path $sdkRoot '.git'))) {
    throw "Missing pinned Meta OpenXR SDK checkout: $sdkRoot"
}
$gitCommand = Get-Command git -CommandType Application -ErrorAction Stop | Select-Object -First 1

function Invoke-SdkPatchGit([string[]]$Arguments, [switch]$Quiet) {
    # Windows PowerShell 5.1 promotes native stderr to terminating errors with
    # ErrorActionPreference=Stop, including the expected failed reverse probe.
    # Capture diagnostics and decide from the actual native exit status instead.
    $ErrorActionPreference = 'Continue'
    $output = @(& $gitCommand.Source -C $sdkRoot @Arguments 2>&1)
    $exitCode = $LASTEXITCODE
    if (-not $Quiet) {
        foreach ($line in $output) { Write-Host $line }
    }
    return $exitCode
}

foreach ($patchName in $patchNames) {
    $patchPath = Join-Path $projectRoot ('patches\' + $patchName)
    if (-not (Test-Path -LiteralPath $patchPath -PathType Leaf)) {
        throw "Missing checked-in third-party patch: $patchPath"
    }
    if ((Invoke-SdkPatchGit -Arguments @('apply', '--reverse', '--check', $patchPath) -Quiet) -eq 0) {
        Write-Host "Meta OpenXR SDK patch already applied: $patchName"
        continue
    }
    if ((Invoke-SdkPatchGit -Arguments @('apply', '--check', $patchPath)) -ne 0) {
        throw "The pinned Meta OpenXR SDK checkout has conflicting edits for $patchName."
    }
    if ((Invoke-SdkPatchGit -Arguments @('apply', $patchPath)) -ne 0) {
        throw "Failed to apply $patchPath"
    }
    Write-Host "Applied Meta OpenXR SDK patch: $patchName"
}
