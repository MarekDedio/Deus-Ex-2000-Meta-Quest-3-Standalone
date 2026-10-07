[CmdletBinding()]
param(
    [string]$GameRoot,
    [string]$MapName = '00_Training',
    [string]$MeshPath,
    [string]$MaterialArrayPath,
    [switch]$AuthoredLighting,
    [switch]$BakedLighting,
    [string]$OutputDirectory,
    [string]$BaselineDirectory,
    [double]$MaxMeanError = 0.0,
    [Nullable[double]]$MinCoverage,
    [single[]]$CameraPosition = @(0.0, 1.65, 0.0),
    [single[]]$YawDegrees = @(0.0, 90.0, 180.0, 270.0),
    [single]$PitchDegrees = 0.0,
    [single]$VerticalFovDegrees = 90.0,
    [ValidateRange(32, 4096)][int]$Width = 1280,
    [ValidateRange(32, 4096)][int]$Height = 720,
    [string]$CompilerPath,
    [string]$CMakePath,
    [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
if (Test-Path variable:PSNativeCommandUseErrorActionPreference) {
    $PSNativeCommandUseErrorActionPreference = $false
}
$projectRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$buildRoot = Join-Path $projectRoot 'desktop\build'
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $projectRoot 'artifacts\desktop-visuals' }
if ($CameraPosition.Count -ne 3) { throw 'CameraPosition needs X, Y and Z in Quest-cache meters.' }
if ($YawDegrees.Count -eq 0) { throw 'At least one YawDegrees value is required.' }
if ([double]::IsNaN($MaxMeanError) -or [double]::IsInfinity($MaxMeanError) -or
    $MaxMeanError -lt 0.0 -or $MaxMeanError -gt 1.0) {
    throw 'MaxMeanError must be between zero and one.'
}
if ($null -ne $MinCoverage -and ([double]::IsNaN($MinCoverage) -or
    [double]::IsInfinity($MinCoverage) -or $MinCoverage -lt 0.0 -or $MinCoverage -gt 1.0)) {
    throw 'MinCoverage must be between zero and one.'
}
if (($MeshPath -and -not $MaterialArrayPath) -or ($MaterialArrayPath -and -not $MeshPath)) {
    throw 'Pass both MeshPath and MaterialArrayPath.'
}
if ($GameRoot -and $MeshPath) { throw 'Pass either GameRoot or existing cache paths.' }
if ($AuthoredLighting -and $MeshPath) {
    throw 'AuthoredLighting requires original GameRoot/map data, not unverified external caches.'
}
if ($BakedLighting -and (-not $GameRoot -or $MeshPath)) {
    throw 'BakedLighting requires original GameRoot/map packages.'
}

function Find-Tool {
    param([string]$Requested, [string]$CommandName, [string[]]$Candidates)
    if ($Requested) {
        if (-not (Test-Path -LiteralPath $Requested -PathType Leaf)) { throw "Tool not found: $Requested" }
        return (Resolve-Path -LiteralPath $Requested).Path
    }
    $command = Get-Command $CommandName -CommandType Application -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if ($command) { return $command.Source }
    foreach ($candidate in $Candidates) {
        if ($candidate -and (Test-Path -LiteralPath $candidate -PathType Leaf)) { return $candidate }
    }
    throw "Cannot find $CommandName. Pass its executable path explicitly."
}
function Invoke-Checked {
    param([string]$Executable, [string[]]$Arguments)
    & $Executable @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Executable failed with exit code $LASTEXITCODE" }
}
function Invariant-Number {
    param([double]$Value)
    return $Value.ToString('G9', [Globalization.CultureInfo]::InvariantCulture)
}

function Resolve-CanonicalArtifactPath {
    param([string]$Path)
    $fullPath = [IO.Path]::GetFullPath($Path)
    $pathRoot = [IO.Path]::GetPathRoot($fullPath)
    $resolved = $pathRoot
    # Resolve existing links component by component, including ancestors of a new output directory.
    foreach ($component in $fullPath.Substring($pathRoot.Length).Split([char[]]@('\', '/'),
            [StringSplitOptions]::RemoveEmptyEntries)) {
        $resolved = Join-Path $resolved $component
        if (Test-Path -LiteralPath $resolved) {
            $item = Get-Item -LiteralPath $resolved -Force
            if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
                if ($item.PSObject.Methods['ResolveLinkTarget']) {
                    $target = $item.ResolveLinkTarget($true)
                    if ($target) { $resolved = $target.FullName }
                } elseif ($item.Target) {
                    # Windows PowerShell 5.1 exposes junction/symlink targets without the newer .NET API.
                    $targetPath = @($item.Target)[0]
                    if (-not [IO.Path]::IsPathRooted($targetPath)) {
                        $targetPath = Join-Path (Split-Path -Parent $resolved) $targetPath
                    }
                    $resolved = [IO.Path]::GetFullPath($targetPath)
                }
            }
        }
    }
    return [IO.Path]::GetFullPath($resolved)
}

$OutputDirectory = Resolve-CanonicalArtifactPath $OutputDirectory
if ($GameRoot) {
    $GameRoot = Resolve-CanonicalArtifactPath (Resolve-Path -LiteralPath $GameRoot).Path
    $gamePrefix = $GameRoot.TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar
    if ($OutputDirectory.Equals($GameRoot, [StringComparison]::OrdinalIgnoreCase) -or
        $OutputDirectory.StartsWith($gamePrefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Desktop artifacts must be outside the original game directory.'
    }
}
if ($BaselineDirectory) {
    $BaselineDirectory = Resolve-CanonicalArtifactPath $BaselineDirectory
    if ($BaselineDirectory.Equals($OutputDirectory, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'BaselineDirectory and OutputDirectory must be different directories.'
    }
}

if (-not $SkipBuild) {
    $CompilerPath = Find-Tool $CompilerPath 'g++.exe' @('C:\ProgramData\mingw64\mingw64\bin\g++.exe')
    $CMakePath = Find-Tool $CMakePath 'cmake.exe' @('D:\Android\Sdk\cmake\3.22.1\bin\cmake.exe')
    $makePath = Join-Path (Split-Path -Parent $CompilerPath) 'mingw32-make.exe'
    if (-not (Test-Path -LiteralPath $makePath)) { throw "MinGW make missing next to compiler: $makePath" }
    # CMake 3.22 writes these values into quoted .cmake strings; normalize Windows separators.
    $cmakeCompiler = $CompilerPath.Replace('\', '/')
    $cmakeMake = $makePath.Replace('\', '/')
    Invoke-Checked $CMakePath @('-S', (Join-Path $projectRoot 'desktop'), '-B', $buildRoot,
        '-G', 'MinGW Makefiles', "-DCMAKE_CXX_COMPILER=$cmakeCompiler", "-DCMAKE_MAKE_PROGRAM=$cmakeMake",
        '-DCMAKE_BUILD_TYPE=Release')
    Invoke-Checked $CMakePath @('--build', $buildRoot, '--parallel', '2')
    $ctest = Join-Path (Split-Path -Parent $CMakePath) 'ctest.exe'
    Invoke-Checked $ctest @('--test-dir', $buildRoot, '--output-on-failure')
}
$captureTool = Join-Path $buildRoot 'deusex_desktop_visual.exe'
if (-not (Test-Path -LiteralPath $captureTool -PathType Leaf)) { throw "Desktop executable missing: $captureTool" }
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null

$mode = 'synthetic-fixture'
$sourceArguments = @('--self-test')
if ($GameRoot) {
    if (-not (Test-Path -LiteralPath (Join-Path $GameRoot "Maps\$MapName.dx"))) {
        throw "Original map is absent: $GameRoot\Maps\$MapName.dx"
    }
    $mode = 'original-game-packages'
    $sourceArguments = @('--game-root', $GameRoot, '--map', $MapName,
        '--cache-root', (Join-Path $OutputDirectory "decoded-cache\$MapName"))
} elseif ($MeshPath) {
    $mode = 'external-quest-cache'
    $MeshPath = (Resolve-Path -LiteralPath $MeshPath).Path
    $MaterialArrayPath = (Resolve-Path -LiteralPath $MaterialArrayPath).Path
    $sourceArguments = @('--mesh', $MeshPath, '--materials', $MaterialArrayPath)
    if ($PSBoundParameters.ContainsKey('MapName')) { $sourceArguments += @('--map', $MapName) }
}
if ($AuthoredLighting) { $sourceArguments += '--authored-lighting' }
if ($BakedLighting) { $sourceArguments += '--baked-lighting' }
$reports = @()
for ($index = 0; $index -lt $YawDegrees.Count; $index++) {
    $fileName = 'view-{0:D2}.bmp' -f $index
    $reportPath = Join-Path $OutputDirectory ('view-{0:D2}.json' -f $index)
    $arguments = @($sourceArguments) + @('--width', "$Width", '--height', "$Height",
        '--camera', (Invariant-Number $CameraPosition[0]), (Invariant-Number $CameraPosition[1]),
        (Invariant-Number $CameraPosition[2]), '--yaw', (Invariant-Number $YawDegrees[$index]),
        '--pitch', (Invariant-Number $PitchDegrees), '--fov', (Invariant-Number $VerticalFovDegrees),
        '--output', (Join-Path $OutputDirectory $fileName), '--report', $reportPath)
    if ($BaselineDirectory) {
        $arguments += @('--baseline', (Join-Path $BaselineDirectory $fileName),
            '--max-mean-error', (Invariant-Number $MaxMeanError))
    }
    if ($null -ne $MinCoverage) { $arguments += @('--min-coverage', (Invariant-Number $MinCoverage)) }
    Invoke-Checked $captureTool $arguments
    $reports += Get-Content -LiteralPath $reportPath -Raw | ConvertFrom-Json
    # Decode each real map once; remaining views consume the exact derived cache.
    if ($GameRoot -and $index -eq 0 -and -not $AuthoredLighting -and -not $BakedLighting) {
        $sourceArguments = @('--map', $MapName, '--mesh', (Join-Path $OutputDirectory "decoded-cache\$MapName\quest-world.mesh"),
            '--materials', (Join-Path $OutputDirectory "decoded-cache\$MapName\quest-material-array.rgba"))
    }
}
$hasWorldView = @($reports | Where-Object { $_.coverageFraction -gt 0.01 }).Count -gt 0
$summary = [ordered]@{
    passed = $hasWorldView
    mode = $mode
    realMapsDecoded = $(if ($GameRoot) { 1 } else { 0 })
    campaignPlayabilityVerified = $false
    scope = $(if ($BakedLighting) {
        'Software world BSP/materials using original static shadow masks, ordered light lists, zone ambient and Unlit; unsupported dynamic lights omitted; no actors, UI, OpenXR, stereo or Quest performance.'
    } elseif ($AuthoredLighting) {
        'Software world BSP/material textures with shared Quest direct vertex lighting approximation; no UE1 lightmaps/shadows, actor meshes, UI, OpenXR, stereo or Quest performance.'
    } else {
        'Software world BSP/material albedo only; no actor meshes, map lights, UI, OpenXR, stereo or Quest performance.'
    })
    lightingMode = $(if ($BakedLighting) { 'original-static-shadow-lightmaps' }
        elseif (-not $AuthoredLighting) { 'albedo-only' }
        elseif ($GameRoot) { 'original-map-direct-vertex-approximation' }
        else { 'synthetic-direct-vertex-fixture' })
    map = $(if ($GameRoot) { $MapName } else { $null })
    captures = @($reports)
}
$summaryPath = Join-Path $OutputDirectory 'summary.json'
$summary | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $summaryPath -Encoding UTF8
Write-Host "Visual capture summary: $summaryPath"
if (-not $hasWorldView) {
    throw 'All configured views are empty. Inspect camera position/map cache and generated reports.'
}
if ($mode -eq 'synthetic-fixture') { Write-Host 'No original game packages supplied: zero real maps tested.' }
