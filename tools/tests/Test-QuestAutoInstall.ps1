[CmdletBinding()]
param()

# Offline only: the substitute accepts a fixed operation allowlist and cannot
# contact USB devices. No real ADB, Gradle build or production APK is used.
$ErrorActionPreference = 'Stop'
$autoHelper = Join-Path $PSScriptRoot '..\Install-QuestBuildIfConnected.ps1'
$manualHelper = Join-Path $PSScriptRoot '..\Install-QuestSmokeTest.ps1'
$mockAdb = Join-Path $PSScriptRoot 'Mock-AdbLifecycle.ps1'
$fixtureRoot = Join-Path ([IO.Path]::GetTempPath()) ('QuestAutoInstallTests-' + [guid]::NewGuid())
New-Item -ItemType Directory -Path $fixtureRoot | Out-Null
$fixtureApk = Join-Path $fixtureRoot 'generated build fixture.apk'
New-Item -ItemType File -Path $fixtureApk | Out-Null
$savedMockState = $env:QUEST_LIFECYCLE_MOCK_STATE
$knownSerial = '2G0YC5ZG620985'
$passed = 0
function Assert-True([bool]$Condition,[string]$Message) { if (-not $Condition) { throw "Assertion failed: $Message" } }
function New-MockState([hashtable]$Overrides = @{}) {
    $state = [ordered]@{
        Devices = "List of devices attached`n${knownSerial}`tdevice"
        ExpectedSerial = $knownSerial; ExpectedApk = $fixtureApk; Model = 'Quest 3'
        FailOperation = ''; EmptyProcessPolls = 0; ProcessPolls = 0
        Launches = 0; Installs = 0; Stops = 0; Commands = @()
    }
    foreach ($key in $Overrides.Keys) { $state[$key] = $Overrides[$key] }
    $env:QUEST_LIFECYCLE_MOCK_STATE = Join-Path $fixtureRoot ('state-' + [guid]::NewGuid() + '.json')
    $state | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $env:QUEST_LIFECYCLE_MOCK_STATE -Encoding utf8
}
function Read-MockState { Get-Content -LiteralPath $env:QUEST_LIFECYCLE_MOCK_STATE -Raw | ConvertFrom-Json }
function Assert-NoMutation {
    $state = Read-MockState
    Assert-True ($state.Installs -eq 0 -and $state.Stops -eq 0 -and $state.Launches -eq 0) 'skipped deployment must not mutate app/device state'
}
function Assert-Throws([scriptblock]$Action,[string]$Pattern) {
    $caught = $null; try { & $Action | Out-Null } catch { $caught = $_ }
    Assert-True ($null -ne $caught -and $caught.Exception.Message -match $Pattern) "failure must be visible and match $Pattern"
}
function Complete-Test([string]$Name) { $script:passed++; Write-Output "PASS: $Name" }
try {
    foreach ($mode in @('ValidateOnly','SkipInstall')) {
        New-MockState
        $parameters = @{ AdbPath = $mockAdb; ApkPath = $fixtureApk }; $parameters[$mode] = $true
        $result = & $autoHelper @parameters
        Assert-True ($result.Status -eq 'Skipped' -and @((Read-MockState).Commands).Count -eq 0) "$mode never contacts ADB"
        Complete-Test "$mode is device-free"
    }
    New-MockState
    $result = & $autoHelper -AdbPath (Join-Path $fixtureRoot 'absent-adb.exe') -ApkPath $fixtureApk
    Assert-True ($result.Status -eq 'Skipped' -and $result.Reason -match 'ADB unavailable') 'missing platform-tools preserves successful build'
    Assert-NoMutation; Complete-Test 'missing ADB skips without claiming installation'
    $cases = @(
        @{ Devices = 'List of devices attached'; Reason = 'absent' },
        @{ Devices = "List of devices attached`n${knownSerial}`tunauthorized"; Reason = 'unauthorized' },
        @{ Devices = "List of devices attached`n${knownSerial}`toffline"; Reason = 'offline' },
        @{ Devices = "List of devices attached`nPHONE_SERIAL`tdevice"; Reason = 'absent' },
        @{ Devices = "List of devices attached`nOTHER_QUEST`tdevice`nPHONE_SERIAL`tdevice"; Reason = 'absent' },
        @{ Devices = "List of devices attached`n${knownSerial}`tdevice`n${knownSerial}`tdevice"; Reason = 'ambiguous' },
        @{ FailOperation = 'Devices'; Reason = 'enumeration failed' },
        @{ Model = 'Pixel 9'; Reason = 'verified as Quest 3' },
        @{ Model = 'Quest 2'; Reason = 'verified as Quest 3' },
        @{ FailOperation = 'Model'; Reason = 'verified as Quest 3' }
    )
    foreach ($case in $cases) {
        New-MockState $case
        $result = & $autoHelper -AdbPath $mockAdb -ApkPath $fixtureApk
        Assert-True ($result.Status -eq 'Skipped' -and $result.Reason -match $case.Reason) 'unsafe/unavailable target skips with exact reason'
        Assert-NoMutation; Complete-Test "unavailable/unsafe target: $($case.Reason)"
    }
    foreach ($devices in @(
        "List of devices attached`n${knownSerial}`tdevice",
        "List of devices attached`nPHONE_SERIAL`tdevice`n${knownSerial}`tdevice"
    )) {
        New-MockState @{ Devices = $devices }
        $result = & $autoHelper -AdbPath $mockAdb -ApkPath $fixtureApk
        $state = Read-MockState
        Assert-True ($result.Status -eq 'Installed' -and $result.Device -ceq $knownSerial -and $result.ApkSha256 -eq (Get-FileHash $fixtureApk).Hash) 'authorized expected Quest returns installed APK identity'
        Assert-True ($state.Installs -eq 1 -and $state.Stops -eq 0 -and $state.Launches -eq 0) 'automatic install issues no explicit force-stop or launch commands'
        Assert-True (@($state.Commands | Where-Object { $_ -ne 'devices' -and $_ -notmatch "^-s $knownSerial " }).Count -eq 0) 'all device operations pin the known Quest'
        Assert-True (@($state.Commands | Where-Object { $_ -match 'uninstall|pm clear|shell rm' }).Count -eq 0) 'replace install never clears saves/data'
        Assert-True (@($state.Commands | Where-Object { $_ -ceq "-s $knownSerial install -r $fixtureApk" }).Count -eq 1) 'space-containing APK path stays one exact argument'
        Complete-Test 'known authorized Quest chosen safely, even alongside another phone'
    }
    New-MockState @{ Devices = "List of devices attached`n${knownSerial}`tdevice`nOTHER_QUEST`tdevice"; ExpectedSerial = 'OTHER_QUEST' }
    $result = & $autoHelper -AdbPath $mockAdb -ApkPath $fixtureApk -DeviceSerial 'OTHER_QUEST'
    Assert-True ($result.Status -eq 'Installed' -and $result.Device -ceq 'OTHER_QUEST' -and (Read-MockState).Installs -eq 1) 'explicit override selects one model-verified Quest'
    Complete-Test 'explicit serial override with multiple connected devices'
    foreach ($failure in @('Install','InstallText')) {
        New-MockState @{ FailOperation = $failure }
        Assert-Throws { & $autoHelper -AdbPath $mockAdb -ApkPath $fixtureApk } 'ADB install failed'
        $state = Read-MockState
        Assert-True ($state.Installs -eq 1 -and $state.Stops -eq 0 -and $state.Launches -eq 0) 'attempted install failure propagates without launch'
        Complete-Test "attempted install fails visibly: $failure"
    }
    New-MockState @{ Model = 'Pixel 9' }
    Assert-Throws { & $manualHelper -AdbPath $mockAdb -ApkPath $fixtureApk -DeviceSerial $knownSerial } 'not a verified Quest 3'
    Assert-NoMutation; Complete-Test 'manual explicit serial cannot target a phone'
    New-MockState
    Assert-Throws { & $autoHelper -AdbPath $mockAdb -ApkPath $fixtureApk -DeviceSerial 'SERIAL;echo' } 'pattern|validate argument'
    Assert-True (@((Read-MockState).Commands).Count -eq 0) 'unsafe explicit serial fails before ADB'
    Complete-Test 'unsafe serial rejected before ADB'
    # Static wiring checks complement helper execution without mocking/running
    # Gradle or claiming that a complete Android build was exercised here.
    $buildText = Get-Content -LiteralPath (Join-Path $PSScriptRoot '..\Build-QuestSmokeTest.ps1') -Raw
    Assert-True ($buildText -match '(?s)if \(\$ValidateOnly\).*?return.*?gradlew\.bat assembleDebug.*?if \(\$LASTEXITCODE -ne 0\).*?throw.*?Install-QuestBuildIfConnected\.ps1') 'build validation exits before Gradle/device work and failed Gradle throws before deploy'
    Assert-True ($buildText -match 'SkipInstall = \$SkipInstall' -and $buildText -match 'if \(\$DeviceSerial\) \{ \$deploymentOptions.DeviceSerial = \$DeviceSerial \}') 'build forwards offline/target options without binding an absent serial'
    Complete-Test 'successful-build-only deployment and validation-only wiring (static)'
    [pscustomobject]@{ Passed = $passed; Failed = 0; HardwareUsed = $false; GradleRun = $false }
} finally {
    if ($null -eq $savedMockState) { Remove-Item Env:QUEST_LIFECYCLE_MOCK_STATE -ErrorAction SilentlyContinue }
    else { $env:QUEST_LIFECYCLE_MOCK_STATE = $savedMockState }
    $resolvedFixture = [IO.Path]::GetFullPath($fixtureRoot)
    $resolvedTemp = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\') + '\'
    if (-not $resolvedFixture.StartsWith($resolvedTemp,[StringComparison]::OrdinalIgnoreCase) -or
        [IO.Path]::GetFileName($resolvedFixture) -notlike 'QuestAutoInstallTests-*') {
        throw "Refusing to clean unexpected fixture location: $resolvedFixture"
    }
    Remove-Item -LiteralPath $resolvedFixture -Recurse -Force
}
