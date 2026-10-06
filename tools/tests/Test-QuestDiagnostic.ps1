[CmdletBinding()]
param()

# No Pester dependency, headset connection, tracking override or real ADB use.
$ErrorActionPreference = 'Stop'
$helper = Join-Path $PSScriptRoot '..\Send-QuestDiagnostic.ps1'
$captureHelper = Join-Path $PSScriptRoot '..\Capture-QuestScreenshot.ps1'
$mockAdb = Join-Path $PSScriptRoot 'Mock-Adb.ps1'
$fixtureRoot = Join-Path ([IO.Path]::GetTempPath()) ('QuestDiagnosticTests-' + [guid]::NewGuid())
New-Item -ItemType Directory -Path $fixtureRoot | Out-Null
$savedMockState = $env:QUEST_DIAGNOSTIC_MOCK_STATE
$passed = 0

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw "Assertion failed: $Message" }
}

function New-MockState([hashtable]$Overrides = @{}) {
    $state = [ordered]@{
        Pending = $null; Consume = $true; PollsBeforeConsume = 1
        CollisionRequest = ''; FailOperation = ''; Inspections = 0
        WriteAttempts = 0; SuccessfulWrites = 0; MapChecks = 0
        ScreenshotDeletes = 0; ReadinessPolls = 0; Pulls = 0
        LastRequest = ''; Commands = @(); ConsumedRequests = @()
    }
    foreach ($key in $Overrides.Keys) { $state[$key] = $Overrides[$key] }
    $env:QUEST_DIAGNOSTIC_MOCK_STATE = Join-Path $fixtureRoot ('state-' + [guid]::NewGuid() + '.json')
    $state | ConvertTo-Json -Depth 6 |
        Set-Content -LiteralPath $env:QUEST_DIAGNOSTIC_MOCK_STATE -Encoding utf8
}

function Read-MockState {
    Get-Content -LiteralPath $env:QUEST_DIAGNOSTIC_MOCK_STATE -Raw | ConvertFrom-Json
}

function Assert-Throws([scriptblock]$Action, [string]$MessagePattern) {
    $caught = $null
    try { & $Action | Out-Null } catch { $caught = $_ }
    Assert-True ($null -ne $caught) 'operation must fail'
    Assert-True ($caught.Exception.Message -match $MessagePattern) (
        "error should match '$MessagePattern'; got '$($caught.Exception.Message)'")
}

function Complete-Test([string]$Name) {
    $script:passed++
    Write-Output "PASS: $Name"
}

try {
    New-MockState
    $result = & $helper -Command menu -TimeoutSeconds 1 -AdbPath $mockAdb
    $state = Read-MockState
    Assert-True ($result.Consumed -eq $true -and $result.Request -eq 'MENU') 'successful request acknowledged'
    Assert-True ($result.Device -eq 'MOCK_QUEST_SERIAL') 'authorized serial propagated'
    Assert-True ($state.SuccessfulWrites -eq 1 -and -not $state.Pending) 'exactly one write consumed'
    Assert-True (@($state.ConsumedRequests).Count -eq 1 -and $state.ConsumedRequests[0] -eq 'MENU') 'app consumed exact token'
    Assert-True (@($state.Commands | Where-Object { $_ -match '^-s MOCK_QUEST_SERIAL shell ' }).Count -ge 3) 'shell calls target selected device'
    Complete-Test 'command consumed successfully, normalized token and serial'

    New-MockState
    $result = & $helper -MapName 00_Training -TimeoutSeconds 1 -AdbPath $mockAdb
    $state = Read-MockState
    Assert-True ($result.Consumed -eq $true -and $state.LastRequest -eq '00_Training') 'map request consumed'
    Assert-True ($state.MapChecks -eq 1) 'original map deployment checked'
    Complete-Test 'deployed map token checked and consumed'

    New-MockState @{ Pending = 'TURNRIGHT'; Consume = $false }
    Assert-Throws { & $helper -Command MENU -TimeoutSeconds 1 -AdbPath $mockAdb } 'left intact'
    $state = Read-MockState
    Assert-True ($state.Pending -eq 'TURNRIGHT' -and $state.WriteAttempts -eq 0) 'pre-existing pending request untouched'
    Complete-Test 'pending mailbox timeout preserves existing request'

    New-MockState @{ Consume = $false }
    Assert-Throws { & $helper -Command SCREENSHOT -TimeoutSeconds 1 -AdbPath $mockAdb } 'left intact'
    $state = Read-MockState
    Assert-True ($state.Pending -eq 'SCREENSHOT' -and $state.SuccessfulWrites -eq 1) 'new unconsumed request retained'
    Complete-Test 'post-queue timeout preserves newly submitted request'

    New-MockState @{ CollisionRequest = 'PICKUP' }
    Assert-Throws { & $helper -Command MENU -TimeoutSeconds 1 -AdbPath $mockAdb } 'existing pending request was preserved'
    $state = Read-MockState
    Assert-True ($state.Pending -eq 'PICKUP' -and $state.WriteAttempts -eq 1 -and $state.SuccessfulWrites -eq 0) 'atomic noclobber collision retained other writer'
    Complete-Test 'noclobber collision between inspection and write'

    foreach ($failure in @('Devices', 'Inspect', 'Queue', 'UnexpectedState')) {
        New-MockState @{ FailOperation = $failure }
        Assert-Throws { & $helper -Command MENU -TimeoutSeconds 1 -AdbPath $mockAdb } 'Could not|Unexpected'
        $state = Read-MockState
        Assert-True ($state.SuccessfulWrites -eq 0 -and @($state.ConsumedRequests).Count -eq 0) "$failure fails closed"
        Complete-Test "ADB $failure failure produces no successful delivery"
    }

    New-MockState @{ FailOperation = 'InspectAfterQueue' }
    Assert-Throws { & $helper -Command MENU -TimeoutSeconds 1 -AdbPath $mockAdb } 'Could not inspect'
    $state = Read-MockState
    Assert-True ($state.Pending -eq 'MENU' -and $state.SuccessfulWrites -eq 1) 'failure after queue preserves request, does not report consumed'
    Complete-Test 'ADB post-queue inspection failure leaves request intact'

    New-MockState @{ FailOperation = 'MapCheck' }
    Assert-Throws { & $helper -MapName 00_Training -TimeoutSeconds 1 -AdbPath $mockAdb } 'not deployed'
    $state = Read-MockState
    Assert-True ($state.MapChecks -eq 1 -and $state.WriteAttempts -eq 0) 'missing map never queued'
    Complete-Test 'ADB map-deployment failure prevents queuing'

    foreach ($invalidCommand in @('INVALID', 'MENU;echo', 'SCREENSHOT extra')) {
        New-MockState
        Assert-Throws { & $helper -Command $invalidCommand -TimeoutSeconds 1 -AdbPath $mockAdb } 'ValidateSet|does not belong|validate argument'
        Assert-True (@((Read-MockState).Commands).Count -eq 0) 'invalid command rejected before ADB'
        Complete-Test "invalid command rejected: $invalidCommand"
    }
    foreach ($invalidMap in @('../00_Training', '00_Training;echo', '00 Training', '00_Training.dx')) {
        New-MockState
        Assert-Throws { & $helper -MapName $invalidMap -TimeoutSeconds 1 -AdbPath $mockAdb } 'pattern|validate argument'
        Assert-True (@((Read-MockState).Commands).Count -eq 0) 'unsafe map token rejected before ADB'
        Complete-Test "invalid map token rejected: $invalidMap"
    }

    New-MockState
    $capturePath = Join-Path $fixtureRoot 'successful-capture.bmp'
    $capture = & $captureHelper -OutputPath $capturePath -TimeoutSeconds 1 -AdbPath $mockAdb
    $state = Read-MockState
    Assert-True ($capture.FullName -eq $capturePath -and $capture.Length -eq 310) 'generated BMP passed actual capture validation'
    Assert-True ($state.LastRequest -eq 'SCREENSHOT' -and $state.SuccessfulWrites -eq 1) 'capture uses consumed diagnostic request'
    Assert-True ($state.ScreenshotDeletes -eq 1 -and $state.ReadinessPolls -eq 1 -and $state.Pulls -eq 1) 'capture goes through all expected ADB phases'
    Complete-Test 'actual screenshot helper succeeds with generated mock BMP'

    foreach ($failure in @('Devices', 'ScreenshotDelete', 'Readiness', 'Pull')) {
        New-MockState @{ FailOperation = $failure }
        $capturePath = Join-Path $fixtureRoot ("failed-$failure.bmp")
        Assert-Throws {
            & $captureHelper -OutputPath $capturePath -TimeoutSeconds 1 -AdbPath $mockAdb
        } 'Could not|could not'
        $state = Read-MockState
        Assert-True (-not (Test-Path -LiteralPath $capturePath)) "$failure cannot yield a successful screenshot"
        if ($failure -eq 'Devices') {
            Assert-True ($state.ScreenshotDeletes -eq 0 -and $state.WriteAttempts -eq 0) 'device failure stops before writes or deletion'
        }
        if ($failure -eq 'Readiness') {
            Assert-True ($state.ReadinessPolls -eq 1 -and $state.Pulls -eq 0) 'ready text with nonzero ADB exit status is rejected'
        }
        Complete-Test "actual screenshot helper ADB $failure failure is fail-closed"
    }

    New-MockState @{ Pending = 'TURNRIGHT'; Consume = $false }
    $capturePath = Join-Path $fixtureRoot 'pending-capture.bmp'
    Assert-Throws {
        & $captureHelper -OutputPath $capturePath -TimeoutSeconds 1 -AdbPath $mockAdb
    } 'left intact'
    $state = Read-MockState
    Assert-True ($state.Pending -eq 'TURNRIGHT' -and $state.WriteAttempts -eq 0 -and $state.Pulls -eq 0) 'capture never overwrites pending diagnostic'
    Complete-Test 'actual screenshot helper preserves pending mailbox request'

    [pscustomobject]@{ Passed = $passed; Failed = 0; HardwareUsed = $false }
} finally {
    if ($null -eq $savedMockState) {
        Remove-Item Env:QUEST_DIAGNOSTIC_MOCK_STATE -ErrorAction SilentlyContinue
    } else {
        $env:QUEST_DIAGNOSTIC_MOCK_STATE = $savedMockState
    }
    # Only remove this exact generated fixture directory, never a caller path.
    $resolvedFixture = [IO.Path]::GetFullPath($fixtureRoot)
    $resolvedTemp = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\') + '\'
    if (-not $resolvedFixture.StartsWith($resolvedTemp, [StringComparison]::OrdinalIgnoreCase) -or
        [IO.Path]::GetFileName($resolvedFixture) -notlike 'QuestDiagnosticTests-*') {
        throw "Refusing to clean unexpected fixture location: $resolvedFixture"
    }
    Remove-Item -LiteralPath $resolvedFixture -Recurse -Force
}
