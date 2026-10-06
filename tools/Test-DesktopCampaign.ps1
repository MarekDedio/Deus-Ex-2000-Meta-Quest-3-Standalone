[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$GameRoot,
    [string[]]$MapNames,
    [single[]]$YawDegrees = @(0.0, 90.0, 180.0, 270.0),
    [single[]]$CameraPosition = @(0.0, 1.65, 0.0),
    [ValidateRange(32, 4096)][int]$Width = 640,
    [ValidateRange(32, 4096)][int]$Height = 360,
    [Nullable[double]]$MinCoverage,
    [string]$OutputDirectory,
    [string]$CompilerPath,
    [string]$CMakePath,
    [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$visualScript = Join-Path $PSScriptRoot 'Test-DesktopVisuals.ps1'
$GameRoot = (Resolve-Path -LiteralPath $GameRoot).Path.TrimEnd('\', '/')
$mapsDirectory = Join-Path $GameRoot 'Maps'
if (-not (Test-Path -LiteralPath $mapsDirectory -PathType Container)) {
    throw "Original Maps directory is absent: $mapsDirectory"
}
if (-not (Test-Path -LiteralPath $visualScript -PathType Leaf)) {
    throw "Desktop capture runner is absent: $visualScript"
}
if ($YawDegrees.Count -eq 0) { throw 'At least one YawDegrees value is required.' }
if ($CameraPosition.Count -ne 3) { throw 'CameraPosition needs X, Y and Z in Quest-cache meters.' }
if ($null -ne $MinCoverage -and ($MinCoverage -lt 0.0 -or $MinCoverage -gt 1.0)) {
    throw 'MinCoverage must be between zero and one.'
}

$availableMaps = @(Get-ChildItem -LiteralPath $mapsDirectory -Filter '*.dx' -File | Sort-Object Name)
if ($availableMaps.Count -eq 0) { throw "No original .dx maps were found in $mapsDirectory" }
$mapLookup = @{}
foreach ($map in $availableMaps) { $mapLookup[$map.BaseName] = $map }
if ($MapNames -and $MapNames.Count -gt 0) {
    $selectedMaps = @()
    $selectedNames = @{}
    foreach ($requestedMap in $MapNames) {
        $mapName = $requestedMap
        if ($mapName.EndsWith('.dx', [StringComparison]::OrdinalIgnoreCase)) {
            $mapName = $mapName.Substring(0, $mapName.Length - 3)
        }
        if (-not $mapLookup.ContainsKey($mapName)) {
            throw "Requested map was not discovered in the original Maps directory: $requestedMap"
        }
        if (-not $selectedNames.ContainsKey($mapName)) {
            $selectedMaps += $mapLookup[$mapName]
            $selectedNames[$mapName] = $true
        }
    }
} else {
    $selectedMaps = $availableMaps
}

if (-not $OutputDirectory) { $OutputDirectory = Join-Path $projectRoot 'artifacts\desktop-campaign' }
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory).TrimEnd('\', '/')
if ($OutputDirectory.Equals($GameRoot, [StringComparison]::OrdinalIgnoreCase) -or
    $OutputDirectory.StartsWith($GameRoot + '\', [StringComparison]::OrdinalIgnoreCase) -or
    $OutputDirectory.StartsWith($GameRoot + '/', [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Campaign artifacts must be outside the original game directory.'
}
# Never reuse a prior decoded cache or overwrite captures from another audit.
$runName = 'run-{0}-{1}' -f (Get-Date -Format 'yyyyMMdd-HHmmss'), ([Guid]::NewGuid().ToString('N').Substring(0, 8))
$runDirectory = Join-Path $OutputDirectory $runName
New-Item -ItemType Directory -Path $runDirectory | Out-Null
$startedUtc = [DateTime]::UtcNow.ToString('o')
$bootstrapError = $null

if (-not $SkipBuild) {
    # A synthetic capture builds and checks the executable once. It is not counted as a real map.
    $bootstrapArguments = @{
        OutputDirectory = (Join-Path $runDirectory '_renderer-bootstrap')
        Width = 64
        Height = 64
        YawDegrees = @([single]0.0)
    }
    if ($CompilerPath) { $bootstrapArguments.CompilerPath = $CompilerPath }
    if ($CMakePath) { $bootstrapArguments.CMakePath = $CMakePath }
    try { & $visualScript @bootstrapArguments }
    catch { $bootstrapError = $_.Exception.Message }
}

$mapResults = @()
foreach ($map in $selectedMaps) {
    $mapOutput = Join-Path $runDirectory $map.BaseName
    $captureLog = Join-Path $mapOutput 'capture.log'
    $errors = @()
    $captureReports = @()
    $imageCount = 0
    if ($bootstrapError) {
        $errors += "Renderer bootstrap failed: $bootstrapError"
    } else {
        Write-Host "Capturing $($map.BaseName) ($($YawDegrees.Count) views)"
        New-Item -ItemType Directory -Path $mapOutput -Force | Out-Null
        $captureArguments = @{
            GameRoot = $GameRoot
            MapName = $map.BaseName
            OutputDirectory = $mapOutput
            Width = $Width
            Height = $Height
            YawDegrees = $YawDegrees
            CameraPosition = $CameraPosition
            SkipBuild = $true
        }
        if ($null -ne $MinCoverage) { $captureArguments.MinCoverage = $MinCoverage }
        try {
            & $visualScript @captureArguments *>&1 |
                Tee-Object -FilePath $captureLog | Out-Host
        } catch {
            $errors += $_.Exception.Message
            "Capture runner failure: $($_.Exception.Message)" |
                Add-Content -LiteralPath $captureLog -Encoding UTF8
        }

        for ($index = 0; $index -lt $YawDegrees.Count; $index++) {
            $reportPath = Join-Path $mapOutput ('view-{0:D2}.json' -f $index)
            $imagePath = Join-Path $mapOutput ('view-{0:D2}.bmp' -f $index)
            if (Test-Path -LiteralPath $imagePath -PathType Leaf) { $imageCount++ }
            else { $errors += "Capture image missing for view $index" }
            if (-not (Test-Path -LiteralPath $reportPath -PathType Leaf)) {
                $errors += "Capture report missing for view $index"
                continue
            }
            try {
                $capture = Get-Content -LiteralPath $reportPath -Raw | ConvertFrom-Json
                if ($null -eq $capture.coverageFraction -or $null -eq $capture.passed) {
                    throw 'The capture report lacks coverageFraction or passed.'
                }
                if (-not $capture.passed) { $errors += "Configured capture gate failed for view $index" }
                $captureReports += $capture
            } catch { $errors += "Invalid capture report for view ${index}: $($_.Exception.Message)" }
        }
    }

    $coverageValues = @($captureReports | ForEach-Object { [double]$_.coverageFraction })
    $coverageStats = $coverageValues | Measure-Object -Minimum -Maximum -Average
    $mapResults += [ordered]@{
        map = $map.BaseName
        source = $map.FullName
        passed = ($errors.Count -eq 0)
        decoded = (@($captureReports | Where-Object { $_.realMapsDecoded -eq 1 }).Count -gt 0)
        requestedCaptures = $YawDegrees.Count
        completedImages = $imageCount
        completedReports = $captureReports.Count
        coverageMinimum = $(if ($coverageValues.Count) { $coverageStats.Minimum } else { $null })
        coverageMaximum = $(if ($coverageValues.Count) { $coverageStats.Maximum } else { $null })
        coverageMean = $(if ($coverageValues.Count) { $coverageStats.Average } else { $null })
        emptyFrames = @($captureReports | Where-Object { $_.emptyFrame }).Count
        nearlyUniformFrames = @($captureReports | Where-Object { $_.nearlyUniformFrame }).Count
        outputDirectory = $mapOutput
        captureLog = $(if (Test-Path -LiteralPath $captureLog -PathType Leaf) { $captureLog } else { $null })
        errors = @($errors | Select-Object -Unique)
        captures = @($captureReports)
    }
}

$failedMaps = @($mapResults | Where-Object { -not $_.passed })
$summary = [ordered]@{
    schemaVersion = 1
    startedUtc = $startedUtc
    finishedUtc = [DateTime]::UtcNow.ToString('o')
    passed = ($failedMaps.Count -eq 0)
    gameRoot = $GameRoot
    availableMaps = $availableMaps.Count
    selectedMaps = $selectedMaps.Count
    realMapsDecoded = @($mapResults | Where-Object { $_.decoded }).Count
    failedMaps = $failedMaps.Count
    completedImages = [long](($mapResults | ForEach-Object { [long]$_.completedImages } | Measure-Object -Sum).Sum)
    completedReports = [long](($mapResults | ForEach-Object { [long]$_.completedReports } | Measure-Object -Sum).Sum)
    width = $Width
    height = $Height
    cameraMeters = @($CameraPosition)
    yawDegrees = @($YawDegrees)
    minimumCoverageGate = $MinCoverage
    bootstrapError = $bootstrapError
    campaignPlayabilityVerified = $false
    scope = 'Original map decoding and software world BSP/material albedo captures only; no actor meshes, map lights, UI, audio, saves, transitions, gameplay, OpenXR, stereo or Quest performance.'
    coverageInterpretation = 'Coverage is the fraction of image pixels occupied by decoded world geometry at one camera position. Empty or uniform frames are reported as warnings unless a minimum coverage gate is requested; they do not prove a map is broken or playable.'
    maps = @($mapResults)
}
$summaryPath = Join-Path $runDirectory 'campaign-report.json'
$summary | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $summaryPath -Encoding UTF8
Write-Host "Campaign decoder/capture report: $summaryPath"
Write-Host "Decoded $($summary.realMapsDecoded)/$($summary.selectedMaps) selected maps; $($summary.completedImages) images; $($summary.failedMaps) map failures."
if ($failedMaps.Count -gt 0) {
    throw "$($failedMaps.Count) map capture checks failed. See $summaryPath"
}
