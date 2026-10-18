[CmdletBinding()]
param()

# Exercise the real initializer against archived pristine source, never the
# developer's dependency worktrees. Fixture-only commits stand in for pins.
$ErrorActionPreference = 'Stop'
if (Test-Path variable:PSNativeCommandUseErrorActionPreference) {
    $PSNativeCommandUseErrorActionPreference = $false
}
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$gitExecutable = (Get-Command git -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$tarExecutable = (Get-Command tar -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$sourceRoot = Join-Path $projectRoot 'third_party\SurrealEngine'
$relativeHeader = 'SurrealEngine/Package/NameString.h'
$sourceHeader = Join-Path $sourceRoot $relativeHeader
$sourceHash = (Get-FileHash -LiteralPath $sourceHeader -Algorithm SHA256).Hash
$fixtureRoot = Join-Path ([IO.Path]::GetTempPath()) ('QuestNameFindPatchTests-' + [guid]::NewGuid())
$fixtureDeps = Join-Path $fixtureRoot 'third_party'
$fixtureEngine = Join-Path $fixtureDeps 'SurrealEngine'
$fixtureMeta = Join-Path $fixtureDeps 'Meta-OpenXR-SDK'
$fixtureHeader = Join-Path $fixtureEngine $relativeHeader
$fixtureHelper = Join-Path $fixtureRoot 'tools\Initialize-ThirdParty.ps1'
$fixturePatch = Join-Path $fixtureRoot 'patches\surreal-engine\NameStringFindCompareIndex.patch'
$archivePath = Join-Path $fixtureRoot 'pristine-name-header.tar'
$pristineHeader = Join-Path $fixtureRoot 'pristine-name-header.h'
$passed = 0

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw "Assertion failed: $Message" }
}
function Invoke-FixtureGit([string]$Checkout, [string[]]$Arguments) {
    $ErrorActionPreference = 'Continue'
    $output = @(& $gitExecutable -C $Checkout @Arguments 2>&1)
    $gitExit = $LASTEXITCODE
    if ($gitExit -ne 0) { throw "Fixture git command failed: $($Arguments -join ' ')`n$($output -join "`n")" }
    return ($output -join "`n").Trim()
}
function Complete-Test([string]$Name) {
    $script:passed++
    Write-Output "PASS: $Name"
}
function Header-Hash { return (Get-FileHash -LiteralPath $fixtureHeader -Algorithm SHA256).Hash }
function Add-FixtureComment {
    # This mechanical mutation belongs only to the generated conflict fixture.
    $text = [IO.File]::ReadAllText($fixtureHeader)
    [IO.File]::WriteAllText($fixtureHeader, $text + "`n// unrelated local fixture edit`n", [Text.UTF8Encoding]::new($false))
}

try {
    New-Item -ItemType Directory -Path $fixtureEngine, $fixtureMeta,
        (Join-Path $fixtureRoot 'tools'), (Split-Path -Parent $fixturePatch) | Out-Null
    Copy-Item -LiteralPath (Join-Path $projectRoot 'tools\Initialize-ThirdParty.ps1') -Destination $fixtureHelper
    Copy-Item -LiteralPath (Join-Path $projectRoot 'patches\surreal-engine\NameStringFindCompareIndex.patch') -Destination $fixturePatch
    $lock = Get-Content -LiteralPath (Join-Path $projectRoot 'third-party-lock.json') -Raw | ConvertFrom-Json
    Invoke-FixtureGit -Checkout $sourceRoot -Arguments @('archive', ('--output=' + $archivePath), $lock.surrealEngineEvaluationFork.commit, '--', $relativeHeader) | Out-Null
    & $tarExecutable -xf $archivePath -C $fixtureEngine
    Assert-True ($LASTEXITCODE -eq 0) 'pristine pinned header extracted into isolated fixture'
    Copy-Item -LiteralPath $fixtureHeader -Destination $pristineHeader
    foreach ($dependency in @(
        @{ Directory = $fixtureMeta; Key = 'metaOpenXrSdk' },
        @{ Directory = $fixtureEngine; Key = 'surrealEngineEvaluationFork' }
    )) {
        Invoke-FixtureGit -Checkout $dependency.Directory -Arguments @('init', '--quiet') | Out-Null
        Invoke-FixtureGit -Checkout $dependency.Directory -Arguments @('add', '--all') | Out-Null
        Invoke-FixtureGit -Checkout $dependency.Directory -Arguments @('-c', 'user.name=Quest patch fixture', '-c', 'user.email=fixture@local', 'commit', '--quiet', '--allow-empty', '-m', 'Pristine isolated dependency fixture') | Out-Null
        Invoke-FixtureGit -Checkout $dependency.Directory -Arguments @('remote', 'add', 'origin', $lock.($dependency.Key).url) | Out-Null
        $lock.($dependency.Key).commit = Invoke-FixtureGit -Checkout $dependency.Directory -Arguments @('rev-parse', 'HEAD')
    }
    [IO.File]::WriteAllText((Join-Path $fixtureRoot 'third-party-lock.json'), ($lock | ConvertTo-Json -Depth 8), [Text.UTF8Encoding]::new($false))

    & $fixtureHelper
    Invoke-FixtureGit -Checkout $fixtureEngine -Arguments @('apply', '--reverse', '--check', $fixturePatch) | Out-Null
    Complete-Test 'fresh pinned source receives the complete tracked API patch'

    $appliedHash = Header-Hash
    & $fixtureHelper
    Assert-True ((Header-Hash) -ceq $appliedHash) 'already-applied header bytes are unchanged'
    Complete-Test 'second initializer run is idempotent'

    Add-FixtureComment
    $dirtyAppliedHash = Header-Hash
    & $fixtureHelper
    Assert-True ((Header-Hash) -ceq $dirtyAppliedHash) 'unrelated edits survive already-applied detection'
    Complete-Test 'already-applied dirty checkout remains byte-identical'

    Copy-Item -LiteralPath $pristineHeader -Destination $fixtureHeader
    Add-FixtureComment
    & $fixtureHelper
    Invoke-FixtureGit -Checkout $fixtureEngine -Arguments @('apply', '--reverse', '--check', $fixturePatch) | Out-Null
    Assert-True ([IO.File]::ReadAllText($fixtureHeader).Contains('// unrelated local fixture edit')) 'unrelated edit survives fresh patch application'
    Complete-Test 'fresh patch applies while preserving unrelated dirty edits'

    Copy-Item -LiteralPath $pristineHeader -Destination $fixtureHeader
    $conflict = [IO.File]::ReadAllText($fixtureHeader).Replace('int GetCompareIndex() const', 'int GetCompareIndex() /* local conflict */ const')
    [IO.File]::WriteAllText($fixtureHeader, $conflict, [Text.UTF8Encoding]::new($false))
    $conflictHash = Header-Hash
    $caught = $null
    try { & $fixtureHelper } catch { $caught = $_ }
    Assert-True ($null -ne $caught -and $caught.Exception.Message -match 'Conflicting edits.*NameStringFindCompareIndex') 'conflict produces an explicit refusal'
    Assert-True ((Header-Hash) -ceq $conflictHash) 'conflict preserves all header bytes, including unapplied hunks'
    Complete-Test 'conflicting dirty edits refuse without partial patching'

    Assert-True ((Get-FileHash -LiteralPath $sourceHeader -Algorithm SHA256).Hash -ceq $sourceHash) 'actual dependency header remains unchanged'
    [pscustomobject]@{ Passed = $passed; Failed = 0; ActualDependencyModified = $false; HardwareUsed = $false }
} finally {
    # Validate the exact private fixture before deleting any material. Remove
    # explicit children deepest-first; never issue a broad recursive deletion.
    $resolvedFixture = [IO.Path]::GetFullPath($fixtureRoot).TrimEnd('\')
    $resolvedTemp = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\') + '\'
    if (-not $resolvedFixture.StartsWith($resolvedTemp, [StringComparison]::OrdinalIgnoreCase) -or
        [IO.Path]::GetFileName($resolvedFixture) -notlike 'QuestNameFindPatchTests-*') {
        throw "Refusing to clean unexpected fixture location: $resolvedFixture"
    }
    if (Test-Path -LiteralPath $resolvedFixture) {
        $children = @(Get-ChildItem -LiteralPath $resolvedFixture -Force -Recurse | Sort-Object { $_.FullName.Length } -Descending)
        foreach ($child in $children) {
            $resolvedChild = [IO.Path]::GetFullPath($child.FullName)
            if (-not $resolvedChild.StartsWith($resolvedFixture + '\', [StringComparison]::OrdinalIgnoreCase)) {
                throw "Refusing to clean unexpected fixture child: $resolvedChild"
            }
            Remove-Item -LiteralPath $resolvedChild -Force
        }
        Remove-Item -LiteralPath $resolvedFixture -Force
    }
}
