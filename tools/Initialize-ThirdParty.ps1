[CmdletBinding()]
param(
    [string]$DependencyRoot,
    [string]$LockFile
)

$ErrorActionPreference = 'Stop'
# Git exit codes are checked below, including when a newer PowerShell enables
# native-command error handling in the caller's profile.
if (Test-Path variable:PSNativeCommandUseErrorActionPreference) {
    $PSNativeCommandUseErrorActionPreference = $false
}
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if (-not $DependencyRoot) { $DependencyRoot = Join-Path $projectRoot 'third_party' }
if (-not $LockFile) { $LockFile = Join-Path $projectRoot 'third-party-lock.json' }
$DependencyRoot = [IO.Path]::GetFullPath($DependencyRoot)
$gitExecutable = (Get-Command git -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$lock = Get-Content -LiteralPath $LockFile -Raw | ConvertFrom-Json

function Invoke-CheckedGit {
    param([string[]]$Arguments)
    $ErrorActionPreference = 'Continue'
    $output = @(& $gitExecutable @Arguments 2>&1)
    $gitExitCode = $LASTEXITCODE
    if ($gitExitCode -ne 0) {
        throw "Git failed (exit $gitExitCode): git $($Arguments -join ' ')`n$($output -join "`n")"
    }
    return $output
}

function Get-GitValue {
    param([string]$Checkout, [string[]]$Arguments)
    $ErrorActionPreference = 'Continue'
    $output = @(& $gitExecutable -C $Checkout @Arguments 2>&1)
    $gitExitCode = $LASTEXITCODE
    return [pscustomobject]@{
        ExitCode = $gitExitCode
        Text = ($output -join "`n").Trim()
    }
}

function Get-NormalizedRemote {
    param([string]$Url)
    return ($Url.Trim().TrimEnd('/').Replace('git@github.com:', 'https://github.com/').ToLowerInvariant() -replace '\.git$', '')
}

$dependencies = @(
    @{ Key = 'metaOpenXrSdk'; Directory = 'Meta-OpenXR-SDK' },
    @{ Key = 'surrealEngineEvaluationFork'; Directory = 'SurrealEngine' }
)

foreach ($dependency in $dependencies) {
    $pin = $lock.($dependency.Key)
    if (-not $pin -or $pin.commit -notmatch '^[0-9a-fA-F]{40}$' -or -not $pin.url) {
        throw "Invalid dependency pin '$($dependency.Key)' in $LockFile. A URL and full 40-character commit are required."
    }
    $commit = $pin.commit.ToLowerInvariant()
    $checkout = Join-Path $DependencyRoot $dependency.Directory
    $restorePending = $false

    $newCheckout = -not (Test-Path -LiteralPath $checkout)
    if (-not $newCheckout -and (Test-Path -LiteralPath $checkout -PathType Container) -and
        @(Get-ChildItem -LiteralPath $checkout -Force).Count -eq 0) {
        $newCheckout = $true
    }
    if ($newCheckout) {
        New-Item -ItemType Directory -Path $checkout -Force | Out-Null
        Invoke-CheckedGit -Arguments @('-C', $checkout, 'init', '--quiet') | Out-Null
        Invoke-CheckedGit -Arguments @('-C', $checkout, 'config', 'questvr.pendingRestore', $commit) | Out-Null
        Invoke-CheckedGit -Arguments @('-C', $checkout, 'remote', 'add', 'origin', $pin.url) | Out-Null
        $restorePending = $true
    } else {
        # Do not let git -C silently discover the enclosing project repository.
        if (-not (Test-Path -LiteralPath (Join-Path $checkout '.git'))) {
            throw "Dependency path already exists without its own Git checkout: $checkout. Its contents were preserved. Move it aside before restoring."
        }
        $topLevel = Get-GitValue -Checkout $checkout -Arguments @('rev-parse', '--show-toplevel')
        if ($topLevel.ExitCode -ne 0 -or [IO.Path]::GetFullPath($topLevel.Text) -ne [IO.Path]::GetFullPath($checkout)) {
            throw "Dependency is not a working checkout at the expected path: $checkout. No files were changed."
        }
        $pending = Get-GitValue -Checkout $checkout -Arguments @('config', '--get', 'questvr.pendingRestore')
        $restorePending = $pending.ExitCode -eq 0 -and $pending.Text -eq $commit
    }

    $remote = Get-GitValue -Checkout $checkout -Arguments @('remote', 'get-url', 'origin')
    if ($remote.ExitCode -ne 0 -or (Get-NormalizedRemote $remote.Text) -ne (Get-NormalizedRemote $pin.url)) {
        throw "Unexpected or missing origin for $checkout. Expected $($pin.url); found $($remote.Text). Checkout preserved."
    }
    $head = Get-GitValue -Checkout $checkout -Arguments @('rev-parse', '--verify', 'HEAD')
    if ($head.ExitCode -ne 0 -or $head.Text -ne $commit) {
        # Only finish a new restore created by this script. An existing checkout,
        # even when clean, is never switched or reset to another revision.
        $changes = Get-GitValue -Checkout $checkout -Arguments @('status', '--porcelain', '--untracked-files=all')
        if (-not $restorePending -or $changes.ExitCode -ne 0 -or $changes.Text) {
            throw "Dependency revision mismatch at $checkout. Required $commit; found $($head.Text). Existing files and edits were preserved. Move the checkout aside or resolve its revision manually."
        }
        Write-Host "Restoring $($dependency.Directory) at $commit ..."
        Invoke-CheckedGit -Arguments @('-C', $checkout, 'fetch', '--depth', '1', '--no-tags', 'origin', $commit) | ForEach-Object { Write-Host $_ }
        Invoke-CheckedGit -Arguments @('-C', $checkout, 'checkout', '--detach', $commit) | ForEach-Object { Write-Host $_ }
    }

    # Missing submodules can be initialized, but existing submodule changes or
    # different revisions must be preserved rather than overwritten by update.
    $submodules = @(Invoke-CheckedGit -Arguments @('-C', $checkout, 'submodule', 'status', '--recursive'))
    foreach ($line in $submodules) {
        if ($line -match '^[+U]') {
            throw "Submodule revision mismatch/conflict in $checkout`: $line. No submodule checkout was changed."
        }
    }
    if (@($submodules | Where-Object { $_ -match '^-' }).Count -gt 0) {
        Invoke-CheckedGit -Arguments @('-C', $checkout, 'submodule', 'update', '--init', '--recursive') | ForEach-Object { Write-Host $_ }
    }
    $verified = Get-GitValue -Checkout $checkout -Arguments @('rev-parse', '--verify', 'HEAD')
    if ($verified.ExitCode -ne 0 -or $verified.Text -ne $commit) {
        throw "Restored dependency failed revision verification: $checkout"
    }
    if ($restorePending) {
        Invoke-CheckedGit -Arguments @('-C', $checkout, 'config', '--unset', 'questvr.pendingRestore') | Out-Null
    }
    $changes = Get-GitValue -Checkout $checkout -Arguments @('status', '--porcelain', '--untracked-files=all')
    if ($changes.ExitCode -ne 0) { throw "Cannot inspect dependency changes at $checkout`: $($changes.Text)" }
    if ($changes.Text) { Write-Host "Verified $($dependency.Directory) pin; existing local modifications preserved." }
    else { Write-Host "Verified $($dependency.Directory) pin (clean)." }
}

Write-Host "Dependencies ready: $DependencyRoot"
