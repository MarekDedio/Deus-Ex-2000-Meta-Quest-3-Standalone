[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$GameRoot,
    [string]$OutputDirectory,
    [ValidateSet('Inventory', 'Health', 'GoalsNotes', 'Logs')][string]$Page = 'Inventory',
    [string[]]$IconNames,
    [ValidateRange(0, 100000)][int]$SelectedIndex = 0,
    [string]$BaselinePath,
    [ValidateRange(0.0, 1.0)][double]$MaxMeanError = 0.0,
    [string]$CompilerPath,
    [string]$CMakePath,
    [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
if (Test-Path variable:PSNativeCommandUseErrorActionPreference) {
    $PSNativeCommandUseErrorActionPreference = $false
}
$projectRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$GameRoot = (Resolve-Path -LiteralPath $GameRoot).Path.TrimEnd('\', '/')
if (-not (Test-Path -LiteralPath (Join-Path $GameRoot 'System\DeusExUI.u') -PathType Leaf)) {
    throw 'GameRoot must contain the original System\DeusExUI.u package.'
}
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $projectRoot 'artifacts\desktop-persona' }
if ($Page -ne 'Inventory' -and ($IconNames.Count -gt 0 -or $SelectedIndex -ne 0)) {
    throw 'Icon fixtures and SelectedIndex apply only to Inventory previews.'
}
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory).TrimEnd('\', '/')
if ($OutputDirectory.Equals($GameRoot, [StringComparison]::OrdinalIgnoreCase) -or
    $OutputDirectory.StartsWith($GameRoot + '\', [StringComparison]::OrdinalIgnoreCase) -or
    $OutputDirectory.StartsWith($GameRoot + '/', [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Persona artifacts must be outside the original game installation.'
}
if (-not $SkipBuild) {
    # Reuse host configuration, build and full CTests; this fixture is not game verification.
    $buildArgs = @{
        OutputDirectory = (Join-Path $projectRoot 'artifacts\_desktop-persona-bootstrap')
        Width = 64
        Height = 64
        YawDegrees = @([single]0.0)
    }
    if ($CompilerPath) { $buildArgs.CompilerPath = $CompilerPath }
    if ($CMakePath) { $buildArgs.CMakePath = $CMakePath }
    & (Join-Path $PSScriptRoot 'Test-DesktopVisuals.ps1') @buildArgs
}
$captureExe = Join-Path $projectRoot 'desktop\build\deusex_desktop_visual.exe'
if (-not (Test-Path -LiteralPath $captureExe -PathType Leaf)) { throw 'Build the desktop visual executable first.' }
$pageSlug = $Page.ToLowerInvariant()
$capturePath = Join-Path $OutputDirectory "persona-$pageSlug.bmp"
$reportPath = Join-Path $OutputDirectory "persona-$pageSlug.json"
$arguments = @('--persona-preview', '--game-root', $GameRoot,
    '--output', $capturePath, '--report', $reportPath, '--persona-page', $Page,
    '--persona-selected', "$SelectedIndex")
foreach ($icon in $IconNames) { $arguments += @('--persona-icon', $icon) }
if ($BaselinePath) {
    $arguments += @('--baseline', $BaselinePath, '--max-mean-error',
        $MaxMeanError.ToString('G9', [Globalization.CultureInfo]::InvariantCulture))
}
& $captureExe @arguments
if ($LASTEXITCODE -ne 0) { throw "Persona preview failed with exit code $LASTEXITCODE" }
$report = Get-Content -LiteralPath $reportPath -Raw | ConvertFrom-Json
$expectedArtworkCount = switch ($Page) { 'Inventory' { 31 } 'Health' { 26 } 'Logs' { 20 } default { 22 } }
if (-not $report.passed -or $report.width -ne 640 -or $report.height -ne 480 -or
    $report.transparentPixels -le 0 -or $report.artworkPaths.Count -ne $expectedArtworkCount -or
    $report.page -ne $Page -or -not $report.fontsAndTextVerified -or $report.fonts.Count -ne 2 -or
    $report.liveRuntimeStateVerified -or $report.glRenderingVerified -or
    $report.openXrVerified -or $report.controllerInteractionVerified -or
    $report.fonts[0].objectPath -ne 'FontMenuHeaders' -or
    $report.fonts[1].objectPath -ne 'FontMenuSmall' -or
    $report.fonts[0].glyphCount -ne 256 -or $report.fonts[1].glyphCount -ne 256 -or
    $report.fonts[0].lineHeight -ne 10 -or $report.fonts[1].lineHeight -ne 10) {
    throw 'Persona preview lacks original artwork/font provenance, transparency, CPU-only scope or expected dimensions.'
}
if ($Page -eq 'Inventory' -and $report.originalInventoryFootprints -and
    ($report.inventoryClassPaths.Count -ne $report.iconPaths.Count -or
     $report.inventoryMetadataSources.Count -lt $report.iconPaths.Count -or
     $report.inventoryOccupiedCells -gt 30 -or
     $report.inventoryPlacements.Count -gt 30 -or
     $report.inventoryDisplayOnlyPacked -ne $report.inventoryPlacements.Count)) {
    throw 'Class-default inventory fixture lacks bounded metadata/placement provenance.'
}
Write-Host "Persona artwork preview: $capturePath"
Write-Host "Persona composition report: $reportPath"
Write-Host 'Preview uses original artwork/fonts and the shared Quest CPU compositor with sample text.'
Write-Host 'GL blending, OpenXR, live runtime state and controller interaction are unverified.'
if ($Page -eq 'Inventory') {
    if ($report.originalInventoryFootprints) {
        Write-Host 'Inventory footprints use actual original class defaults; positions are detached display-only packing, not a saved inventory.'
    } else {
        Write-Host 'Unrecognized icon fixtures use the legacy one-cell asset-only fallback, not original item footprints or a saved inventory.'
    }
}
