[CmdletBinding()]
param()

# No Pester dependency, headset connection, real ADB or installed app changes.
$ErrorActionPreference = 'Stop'
$launchHelper = Join-Path $PSScriptRoot '..\Launch-QuestSmokeTest.ps1'
$installHelper = Join-Path $PSScriptRoot '..\Install-QuestSmokeTest.ps1'
$mockAdb = Join-Path $PSScriptRoot 'Mock-AdbLifecycle.ps1'
$fixtureRoot = Join-Path ([IO.Path]::GetTempPath()) ('QuestLifecycleTests-' + [guid]::NewGuid())
New-Item -ItemType Directory -Path $fixtureRoot | Out-Null
$fixtureApk = Join-Path $fixtureRoot 'generated test fixture.apk'
# An empty fixture is sufficient: the install helper delegates APK validation
# to ADB. The mock verifies the exact path/replace option instead of installing.
New-Item -ItemType File -Path $fixtureApk | Out-Null
$savedMockState = $env:QUEST_LIFECYCLE_MOCK_STATE
$passed = 0

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw "Assertion failed: $Message" }
}
function New-MockState([hashtable]$Overrides = @{}) {
    $state = [ordered]@{
        Devices = "List of devices attached`nMOCK_QUEST_SERIAL`tdevice"
        ExpectedSerial = 'MOCK_QUEST_SERIAL'; ExpectedApk = $fixtureApk
        FailOperation = ''; EmptyProcessPolls = 0; ProcessPolls = 0
        Launches = 0; Installs = 0; Stops = 0; Commands = @()
    }
    foreach ($key in $Overrides.Keys) { $state[$key] = $Overrides[$key] }
    $env:QUEST_LIFECYCLE_MOCK_STATE = Join-Path $fixtureRoot ('state-' + [guid]::NewGuid() + '.json')
    $state | ConvertTo-Json -Depth 6 |
        Set-Content -LiteralPath $env:QUEST_LIFECYCLE_MOCK_STATE -Encoding utf8
}
function Read-MockState {
    Get-Content -LiteralPath $env:QUEST_LIFECYCLE_MOCK_STATE -Raw | ConvertFrom-Json
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
function Assert-NoMutation {
    $state = Read-MockState
    Assert-True ($state.Installs -eq 0 -and $state.Stops -eq 0 -and $state.Launches -eq 0) 'failed preflight must not mutate device/app state'
}

try {
    New-MockState
    $result = & $launchHelper -AdbPath $mockAdb
    $state = Read-MockState
    Assert-True ($result.Device -ceq 'MOCK_QUEST_SERIAL' -and $result.ProcessIds -eq '1234 1235') 'device and PID proof returned'
    Assert-True ($state.Launches -eq 1 -and $state.ProcessPolls -eq 1) 'exactly one waited launch, then process query'
    Assert-True ($result.State -match 'map/tracking readiness is separate') 'launch result does not claim rendered/tracked readiness'
    Complete-Test 'single waited launch with explicit point-in-time process proof'

    New-MockState @{ EmptyProcessPolls = 1 }
    $null = & $launchHelper -AdbPath $mockAdb -StartupTimeoutSeconds 1
    $state = Read-MockState
    Assert-True ($state.Launches -eq 1 -and $state.ProcessPolls -eq 2) 'startup delay polls process without another intent'
    Complete-Test 'delayed process appearance does not repeat launch intent'

    $multipleDevices = "List of devices attached`nOTHER_QUEST`tdevice`nMOCK_QUEST_SERIAL`tdevice"
    foreach ($helper in @($launchHelper, $installHelper)) {
        New-MockState @{ Devices = $multipleDevices }
        $parameters = @{ AdbPath = $mockAdb; DeviceSerial = 'MOCK_QUEST_SERIAL' }
        if ($helper -eq $installHelper) { $parameters.ApkPath = $fixtureApk }
        $null = & $helper @parameters
        $state = Read-MockState
        Assert-True ($state.Launches -eq 1) 'explicit authorized serial selects one device'
        Assert-True (@($state.Commands | Where-Object { $_ -ne 'devices' -and $_ -notmatch '^-s MOCK_QUEST_SERIAL ' }).Count -eq 0) 'every device operation pins chosen serial'
        Complete-Test "explicit serial amid multiple devices: $(Split-Path $helper -Leaf)"
    }

    foreach ($helper in @($launchHelper, $installHelper)) {
        foreach ($deviceList in @(
            'List of devices attached',
            "List of devices attached`nMOCK_QUEST_SERIAL`tunauthorized",
            "List of devices attached`nMOCK_QUEST_SERIAL`toffline",
            $multipleDevices
        )) {
            New-MockState @{ Devices = $deviceList }
            $parameters = @{ AdbPath = $mockAdb }
            if ($helper -eq $installHelper) { $parameters.ApkPath = $fixtureApk }
            Assert-Throws { & $helper @parameters } 'Expected exactly one authorized'
            Assert-NoMutation
            Complete-Test "ambiguous/unauthorized/offline/absent preflight: $(Split-Path $helper -Leaf)"
        }
        foreach ($requestedSerial in @('NOT_CONNECTED', 'mock_quest_serial')) {
            New-MockState
            $parameters = @{ AdbPath = $mockAdb; DeviceSerial = $requestedSerial }
            if ($helper -eq $installHelper) { $parameters.ApkPath = $fixtureApk }
            Assert-Throws { & $helper @parameters } 'not connected and authorized'
            Assert-NoMutation
            Complete-Test "explicit serial requires exact authorized match: $(Split-Path $helper -Leaf)"
        }
        New-MockState @{ FailOperation = 'Devices' }
        $parameters = @{ AdbPath = $mockAdb }
        if ($helper -eq $installHelper) { $parameters.ApkPath = $fixtureApk }
        Assert-Throws { & $helper @parameters } 'Could not list ADB devices'
        Assert-NoMutation
        Complete-Test "nonzero devices exit with apparently valid output: $(Split-Path $helper -Leaf)"

        New-MockState
        $parameters = @{ AdbPath = $mockAdb; DeviceSerial = 'SERIAL;echo' }
        if ($helper -eq $installHelper) { $parameters.ApkPath = $fixtureApk }
        Assert-Throws { & $helper @parameters } 'pattern|validate argument'
        Assert-True (@((Read-MockState).Commands).Count -eq 0) 'invalid serial rejected before ADB'
        Complete-Test "unsafe serial rejected before ADB: $(Split-Path $helper -Leaf)"
    }

    foreach ($failure in @('Launch', 'LaunchStatus', 'Process', 'ProcessText')) {
        New-MockState @{ FailOperation = $failure }
        Assert-Throws { & $launchHelper -AdbPath $mockAdb -StartupTimeoutSeconds 1 } 'launch failed|no running app process'
        $state = Read-MockState
        Assert-True ($state.Launches -eq 1) 'failure never repeats launch'
        Complete-Test "launch/process failure is fail-closed: $failure"
    }

    New-MockState
    $null = & $installHelper -AdbPath $mockAdb -ApkPath $fixtureApk
    $state = Read-MockState
    Assert-True ($state.Installs -eq 1 -and $state.Stops -eq 1 -and $state.Launches -eq 1) 'replace install, stop, one launch'
    Assert-True (@($state.Commands | Where-Object { $_ -match 'uninstall|pm clear|shell rm|install -r -' }).Count -eq 0) 'no uninstall/data clearing/removal options'
    Assert-True ($state.Commands[1] -ceq "-s MOCK_QUEST_SERIAL install -r $fixtureApk") 'space-containing APK path is one exact argument'
    Complete-Test 'install targets authorized serial with -r and preserves app data'

    # Test the real default path without needing an existing production APK.
    # Copy unchanged helpers into a generated project-shaped fixture instead.
    $defaultTools = Join-Path $fixtureRoot 'project\tools'
    $defaultApkDirectory = Join-Path $fixtureRoot 'project\android\build\outputs\apk\debug'
    New-Item -ItemType Directory -Path $defaultTools, $defaultApkDirectory | Out-Null
    Copy-Item -LiteralPath $launchHelper, $installHelper -Destination $defaultTools
    $defaultApk = Join-Path $defaultTools '..\android\build\outputs\apk\debug\DeusExQuestVrSmokeTest-debug.apk'
    New-Item -ItemType File -Path $defaultApk | Out-Null
    New-MockState @{ ExpectedApk = $defaultApk }
    $null = & (Join-Path $defaultTools 'Install-QuestSmokeTest.ps1') -AdbPath $mockAdb
    $state = Read-MockState
    Assert-True ($state.Installs -eq 1 -and $state.Launches -eq 1) 'unmodified default path resolves relative to script location'
    Complete-Test 'default APK path works without parameter-initializer script-root assumptions'

    foreach ($failure in @('Install', 'Stop')) {
        New-MockState @{ FailOperation = $failure }
        Assert-Throws { & $installHelper -AdbPath $mockAdb -ApkPath $fixtureApk } 'install failed|Could not stop'
        $state = Read-MockState
        Assert-True ($state.Installs -eq 1 -and $state.Launches -eq 0) 'failed install/stop cannot report launched'
        if ($failure -eq 'Install') { Assert-True ($state.Stops -eq 0) 'failed install does not stop previous instance' }
        Complete-Test "install workflow aborts after nonzero exit: $failure"
    }
    New-MockState
    Assert-Throws { & $installHelper -AdbPath $mockAdb -ApkPath (Join-Path $fixtureRoot 'absent.apk') } 'APK not found'
    Assert-True (@((Read-MockState).Commands).Count -eq 0) 'missing APK rejected before ADB'
    Complete-Test 'missing APK fails before device operations'
    New-MockState
    Assert-Throws { & $launchHelper -AdbPath (Join-Path $fixtureRoot 'absent-adb.exe') } 'ADB not found'
    Assert-True (@((Read-MockState).Commands).Count -eq 0) 'missing ADB produces no commands'
    Complete-Test 'missing ADB fails before launch'

    [pscustomobject]@{ Passed = $passed; Failed = 0; HardwareUsed = $false }
} finally {
    if ($null -eq $savedMockState) {
        Remove-Item Env:QUEST_LIFECYCLE_MOCK_STATE -ErrorAction SilentlyContinue
    } else {
        $env:QUEST_LIFECYCLE_MOCK_STATE = $savedMockState
    }
    # Remove only this exact generated fixture directory after checking its
    # resolved absolute path remains inside the platform's temporary directory.
    $resolvedFixture = [IO.Path]::GetFullPath($fixtureRoot)
    $resolvedTemp = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\') + '\'
    if (-not $resolvedFixture.StartsWith($resolvedTemp, [StringComparison]::OrdinalIgnoreCase) -or
        [IO.Path]::GetFileName($resolvedFixture) -notlike 'QuestLifecycleTests-*') {
        throw "Refusing to clean unexpected fixture location: $resolvedFixture"
    }
    Remove-Item -LiteralPath $resolvedFixture -Recurse -Force
}
