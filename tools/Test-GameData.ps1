[CmdletBinding()]
param(
    [Parameter()]
    [string]$GameRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
)

$requiredFiles = @(
    'System\DeusEx.u',
    'System\DeusExCharacters.u',
    'System\DeusExConversations.u',
    'Maps\00_Training.dx',
    'Textures\CoreTexMetal.utx',
    'Music\Training_Music.umx'
)

$missing = foreach ($relativePath in $requiredFiles) {
    if (-not (Test-Path -LiteralPath (Join-Path $GameRoot $relativePath))) {
        $relativePath
    }
}

$packages = Get-ChildItem -LiteralPath $GameRoot -Recurse -File -ErrorAction Stop |
    Where-Object Extension -In @('.dx', '.u', '.utx', '.uax', '.umx')

$result = [PSCustomObject]@{
    GameRoot = (Resolve-Path $GameRoot).Path
    RequiredFilesPresent = ($missing.Count -eq 0)
    MissingRequiredFiles = @($missing)
    PackageCount = $packages.Count
    PackageBytes = ($packages | Measure-Object Length -Sum).Sum
}

$result | ConvertTo-Json -Depth 3

if ($missing.Count -ne 0) {
    exit 1
}
